//
// Created by Yihua Liu on 2024/6/4.
//

#include <stdexcept>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include "xlsxabstractsheet.h"
#include "xlsxcelllocation.h"
#include "xlsxdocument.h"
#include "xlsxworkbook.h"

#include "IniConfigParser.h"
#include "MaterialDbModel.h"
#include "ParameterSystem.h"

MaterialDbModel::MaterialDbModel(QObject *parent) : QAbstractListModel(parent) {}

int MaterialDbModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return static_cast<int>(m_list.count());
}

QVariant MaterialDbModel::data(const QModelIndex &index, int role) const {
    const QMap<QString, OpticMaterial *>::const_iterator it = m_list.begin() + index.row();
    const OpticMaterial *mat = it.value();
    switch (role) {
        case NameRole:
            return it.key();
        case ValueRole:
            return QVariant::fromValue(it.value()->nWl());
        default:
            return {};
    }
}

double MaterialDbModel::getProgress() const {
    return import_progress;
}

void MaterialDbModel::setProgress(int progress) {
    if (import_progress not_eq progress) {
        import_progress = progress;
        emit progressChanged(import_progress);
    }
}

QString findSolcoreUserConfig() {
    /* Let us expect P1031R2: Low level file i/o library (https://wg21.link/p1031r2)
     * https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1031r2.pdf (https://ned14.github.io/llfio/)
    #ifdef _MSC_VER
    char *user_path;
    std::size_t len_userdata;
    errno_t err_userdata = _dupenv_s(&user_path, &len_userdata, "SUIS_USER_DATA");
    if (err_userdata) {
        throw std::runtime_error("_dupenv_s fails to get user data environment variable, errno is " + std::to_string(err_userdata) + ".");
    }
    #else
    char *user_path = std::getenv("SUIS_USER_DATA");
    #endif
    if (!user_path) {
        throw std::runtime_error("getenv fails to get user data environment variable.");
    }
    free(user_path);
     */
    const QProcessEnvironment sysenv = QProcessEnvironment::systemEnvironment();
    QString user_path_str = sysenv.value("SOLCORE_USER_DATA", "");
    QDir user_path;
    if (user_path_str.isEmpty()) {
        qDebug("SOLCORE_USER_DATA does not exist or is empty.");
        // the same as QDir::homePath()
        user_path = QStandardPaths::displayName(QStandardPaths::HomeLocation);
        user_path_str = user_path.filePath(".solcore");
        user_path = user_path_str;
    } else {
        qDebug("Found non-empty SOLCORE_USER_DATA path.");
        user_path = user_path_str;
    }
    // std::filesystem::path::format has native_format, generic_format, and auto_format.
    // generic_format uses slashes, while native_format of Windows paths uses backslashes.
    // We can convert std::filesystem::path to std::string by std::filesystem::path::generic_string() bu
    // QDir() constructor accepts both QString and std::filesystem::path
    // if (not user_path.exists()) {
        // QDir::mkdir() is enough. Alternatively use QDir::mkpath() to create all parent directories.
        // user_path.mkdir(user_path_str);
    // }
    return user_path.filePath("solcore_config.txt");
}

QVariantMap MaterialDbModel::readSolcoreDb(const QString& db_path) {
    const QUrl url(db_path);
    const QString user_config = findSolcoreUserConfig();
    QFile ini_file;
    if (QFile::exists(user_config)) {
        qDebug() << "Using user configuration file " << user_config;
        ini_file.setFileName(user_config);
    } else {
        ini_file.setFileName(url.toLocalFile());
    }
    const QFileInfo ini_finfo(ini_file);
    QVariantMap result;
    QStringList mat_list;
    if (not ini_file.open(QIODevice::ReadOnly)) {
        qWarning("Cannot open the configuration ini file %s.", qUtf8Printable(db_path));
        result["status"] = 1;
        result["matlist"] = mat_list;
        return result;
    }
    // db_dir.setFilter(QDir::Dirs);
    // QStringList name_filters;
    // name_filters << "*-Material";
    // db_dir.setNameFilters(name_filters);
    // for (const QFileInfo& subdir_info : subdir_list) {
    //     const QString mat_name = subdir_info.fileName().split('-').front();
    IniConfigParser solcore_config(ini_file.fileName());
    const ParameterSystem par_sys(solcore_config.loadGroup("Parameters"), ini_finfo.absolutePath());
    const QMap<QString, QString> mat_map = solcore_config.loadGroup("Materials");
    for (QMap<QString, QString>::const_iterator it = mat_map.constBegin(); it not_eq mat_map.constEnd(); it++) {
        try {
            const QString& mat_name = it.key();
            const QDir mat_dir = it.value();
            if (par_sys.isComposition(mat_name, "x")) {
                const QDir n_dir = mat_dir.filePath("n");
                const QDir k_dir = mat_dir.filePath("k");
                if (not n_dir.exists() or not k_dir.exists()) {
                    throw std::runtime_error("Cannot find n and k folder for composition material " + mat_name.toStdString());
                }
                const QFileInfoList n_flist = n_dir.entryInfoList();
                const QFileInfoList k_flist = k_dir.entryInfoList();
                std::vector<std::pair<double, std::vector<double>>> n_wl;
                std::vector<std::pair<double, std::vector<double>>> n_data;
                std::vector<std::pair<double, std::vector<double>>> k_wl;
                std::vector<std::pair<double, std::vector<double>>> k_data;
                for (const QFileInfo& n_info : n_flist) {
                    // not_eq "critical_points"
                    const QString main_fraction_str = n_info.baseName().split('_').front();
                    mat_list.append(mat_name + main_fraction_str);
                    QFile n_file(n_info.fileName());
                    if (not n_file.open(QIODevice::ReadOnly)) {
                        throw std::runtime_error("Cannot open file " + n_info.fileName().toStdString());
                    }
                    QTextStream n_stream(&n_file);
                    std::vector<double> frac_n_wl;
                    std::vector<double> frac_n_data;
                    while (not n_stream.atEnd()) {
                        const QString line = n_stream.readLine();
                        // Clazy: Don't create temporary QRegularExpression objects.
                        // Use a static QRegularExpression object instead
                        static const QRegularExpression ws_regexp("\\s+");
                        const QStringList ln_data = line.split(ws_regexp);
                        if (ln_data.length() not_eq 2) {
                            throw std::runtime_error("Error parsing file " + n_info.fileName().toStdString());
                        }
                        frac_n_wl.push_back(ln_data.front().toDouble());
                        frac_n_data.push_back(ln_data.back().toDouble());
                    }
                    n_file.close();
                    n_wl.emplace_back(main_fraction_str.toDouble(), frac_n_wl);
                    n_data.emplace_back(main_fraction_str.toDouble(), frac_n_data);
                }
                for (const QFileInfo& k_info : k_flist) {
                    // not_eq "critical_points"
                    const QString main_fraction_str = k_info.baseName().split('_').front();
                    mat_list.append(mat_name + main_fraction_str);
                    QFile k_file(k_info.fileName());
                    if (not k_file.open(QIODevice::ReadOnly)) {
                        throw std::runtime_error("Cannot open file " + k_info.fileName().toStdString());
                    }
                    QTextStream k_stream(&k_file);
                    std::vector<double> frac_k_wl;
                    std::vector<double> frac_k_data;
                    while (not k_stream.atEnd()) {
                        const QString line = k_stream.readLine();
                        static const QRegularExpression ws_regexp("\\s+");
                        const QStringList ln_data = line.split(ws_regexp);
                        if (ln_data.length() not_eq 2) {
                            throw std::runtime_error("Error parsing file " + k_info.fileName().toStdString());
                        }
                        frac_k_wl.push_back(ln_data.front().toDouble());
                        frac_k_data.push_back(ln_data.back().toDouble());
                    }
                    k_file.close();
                    k_wl.emplace_back(main_fraction_str.toDouble(), frac_k_wl);
                    k_data.emplace_back(main_fraction_str.toDouble(), frac_k_data);
                }
                CompOpticMaterial opt_mat(mat_name, n_wl, n_data, k_wl, k_data);
                m_comp_list.insert(mat_name, &opt_mat);
            }
            setProgress(static_cast<int>(std::distance(mat_map.constBegin(), it) / mat_map.size()));
        } catch (std::runtime_error& e) {
            qWarning(e.what());
            result["status"] = 2;
            result["matlist"] = mat_list;
            return result;
        }
    }
    result["status"] = 0;
    result["matlist"] = mat_list;
    return result;
}

QVariantMap MaterialDbModel::readDfDb(const QString& db_path) {
    const QUrl url(db_path);
    QString db_path_imported = db_path;
    if (url.isLocalFile()) {
        db_path_imported = QDir::toNativeSeparators(url.toLocalFile());
    }
    QXlsx::Document doc(db_path_imported);
    QVariantMap result;
    QStringList mat_list;
    if (not doc.load()) {
        qWarning("Cannot load DriftFusion's material data file %s ", qUtf8Printable(db_path));
        result["status"] = 1;
        result["matlist"] = mat_list;
        return result;
    }
    doc.selectSheet("data");
    // QXlsx::AbstractSheet is not a derived class of QObject
    QXlsx::AbstractSheet *data_sheet = doc.sheet("data");
    if (data_sheet == nullptr) {
        qWarning("Data sheet in data file %s does not exist!", qUtf8Printable(db_path));
        result["status"] = 2;
        result["matlist"] = mat_list;
        return result;
    }
    data_sheet->workbook()->setActiveSheet(0);
    auto *wsheet = (QXlsx::Worksheet *)data_sheet->workbook()->activeSheet();
    if (wsheet == nullptr) {
        qWarning("Data sheet not found");
        result["status"] = 2;
        result["matlist"] = mat_list;
        return result;
    }
    int maxRow = wsheet->dimension().rowCount();  // qsizetype is long long (different from std::size_t)
    int maxCol = wsheet->dimension().columnCount();
    // QVector is an alias for QList.
    // QMapIterator<int, QMap<int, std::shared_ptr<Cell>>> iterates by rows
    // QList<QXlsx::CellLocation> clList = wsheet->getFullCells(&maxRow, &maxCol);
    // This approach costs more time, less space.
    std::vector<double> wls(maxRow - 1);  // header by default
    for (int rc = 2; rc <= maxRow; rc++) {
        // QXlsx::Worksheet::cellAt() uses QMap find
        QXlsx::Cell *cell = wsheet->cellAt(rc, 1);
        if (cell not_eq nullptr) {
            // QXlsx::Cell::readValue() will keep formula text!
            wls[rc - 2] = cell->value().toDouble() / 1e-9;
        }  // qDebug() << "Empty cell at Row " << rc << " Column " << 0;
    }
    setProgress(1 / maxCol);
    for (int cc = 2; cc < maxCol; cc += 2) {
        std::vector<double> n_list(maxRow - 1);
        std::vector<double> k_list(maxRow - 1);
        // const QString mat_name = clList.at(cc).cell->readValue().toString();
        const QString mat_name = wsheet->cellAt(1, cc)->readValue().toString().split('_').front();
        mat_list.push_back(mat_name);
        for (int rc = 2; rc <= maxRow; rc++) {
            QXlsx::Cell *cell = wsheet->cellAt(rc, cc);
            // std::shared_ptr<QXlsx::Cell> cell = clList.at(rc * maxCol + cc).cell;
            if (cell not_eq nullptr) {
                n_list[rc - 2] = cell->readValue().toDouble();
            }  // qDebug() << "Empty cell at Row " << rc << " Column " << cc;
            cell = wsheet->cellAt(rc, cc + 1);
            if (cell not_eq nullptr) {
                k_list[rc - 2] = cell->readValue().toDouble();
            }  // qDebug() << "Empty cell at Row " << rc << " Column " << cc + 1;
        }
        std::vector<double> n_wl = {wls.begin(), wls.begin() + static_cast<std::vector<double>::difference_type>(n_list.size())};
        std::vector<double> k_wl = {wls.begin(), wls.begin() + static_cast<std::vector<double>::difference_type>(k_list.size())};
        // In template: no matching function for call to 'construct_at'
        OpticMaterial opt_mat(mat_name, n_wl, n_list, k_wl, k_list);
        m_list.insert(mat_name, &opt_mat);
        setProgress((cc + 1) / maxCol);
    }
    result["status"] = 0;
    result["matlist"] = mat_list;
    return result;
}
