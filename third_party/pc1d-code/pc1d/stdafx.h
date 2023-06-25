/* PC1D Semiconductor Device Simulator
Copyright (C) 2003 University of New South Wales
Authors: Paul A. Basore, Donald A. Clugston

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

//#include "AggressiveOptimize.h" // Reduce exe size
#define WINVER 0x0400
#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions (including VB)
#include <afxtempl.h>
#include "math.h"

// the following is from ScientificDDX.cpp
void AFXAPI DDX_ScientificDouble(CDataExchange* pDX, int nIDC, double& value);
void AFXAPI DDV_MinMaxSciDouble(CDataExchange* pDX, double const& value, double minVal, double maxVal);
void GetScientificDoubleDisplayRange(double &lo, double &hi);
void SetScientificDoubleDisplayRange(double lo, double hi);
void FormatScientificDouble(char *buffer, double value);

