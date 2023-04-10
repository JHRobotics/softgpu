/******************************************************************************
 * Copyright (c) 2023 Jaroslav Hensl                                          *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person                *
 * obtaining a copy of this software and associated documentation             *
 * files (the "Software"), to deal in the Software without                    *
 * restriction, including without limitation the rights to use,               *
 * copy, modify, merge, publish, distribute, sublicense, and/or sell          *
 * copies of the Software, and to permit persons to whom the                  *
 * Software is furnished to do so, subject to the following                   *
 * conditions:                                                                *
 *                                                                            *
 * The above copyright notice and this permission notice shall be             *
 * included in all copies or substantial portions of the Software.            *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,            *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES            *
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                   *
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT                *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,               *
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING               *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR              *
 * OTHER DEALINGS IN THE SOFTWARE.                                            *
 *                                                                            *
*******************************************************************************/
#ifndef __ACTION_H__INCLUDED__
#define __ACTION_H__INCLUDED__

typedef BOOL (*action_start_f)(HWND hwnd);
typedef BOOL (*action_process_f)(HWND hwnd);
typedef BOOL (*action_end_f)(HWND hwnd);

#define ACTION_NAME_MAX 200

typedef struct install_action
{
	char              name[ACTION_NAME_MAX];
	action_start_f    start;
	action_process_f  process;
	action_end_f      end;
	struct install_action *next;
} install_action_t;

void softgpu_done(HWND hwnd);
void softgpu_reset(HWND hwnd);
void softgpu_info(HWND hwnd, const char *part);

#define TIMER_ID    15

void timer_proc(HWND hwnd);

BOOL action_create(const char *name, action_start_f astart, action_process_f fprocess, action_end_f fend);

/* softgpu actions */
BOOL driver_without_setupapi(HWND hwnd);
BOOL proc_wait(HWND hwnd);
BOOL mscv_start(HWND hwnd);
BOOL dx_start(HWND hwnd);
BOOL ie_start(HWND hwnd);
BOOL setup_end(HWND hwnd);
BOOL driver_install(HWND hwnd);
void setInstallPath(HWND input);
void setInstallSrc(const char *path);

BOOL gl95_start(HWND hwnd);

BOOL filescopy_start(HWND hwnd);
BOOL filescopy_wait(HWND hwnd);
BOOL filescopy_result(HWND hwnd);

extern BOOL install_wine;
extern BOOL install_glide;

BOOL simd95(HWND hwnd);
BOOL infFixer(HWND hwnd);

BOOL apply_reg_fixes(HWND hwnd);

void setDXpath(const char *dx);
void setIEpath(const char *ie);

#endif /* __ACTION_H__INCLUDED__ */
