/*
*  Copyright (C) 2007-2012 Jolien Creighton, Drew Keppel
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with with program; see the file COPYING. If not, write to the
*  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
*  MA  02110-1301  USA
*/

#ifndef _FFTWMUTEX_H
#define _FFTWMUTEX_H

#include <lal/LALConfig.h>

#ifdef  __cplusplus
extern "C" {
#endif

void XLALFFTWWisdomLock(void);
void XLALFFTWWisdomUnlock(void);

#if defined(LAL_PTHREAD_LOCK) && defined(LAL_FFTW3_ENABLED)
# define LAL_FFTW_WISDOM_LOCK XLALFFTWWisdomLock()
# define LAL_FFTW_WISDOM_UNLOCK XLALFFTWWisdomUnlock()
#else
# define LAL_FFTW_WISDOM_LOCK
# define LAL_FFTW_WISDOM_UNLOCK
#endif

#ifdef  __cplusplus
}
#endif

#endif /* _FFTWMUTEX_H */
