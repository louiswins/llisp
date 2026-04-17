#define WIN32_LEAN_AND_MEAN
#include "resource.h"
#include <windows.h>

// windows.h and myself both want to define these
#undef TRUE
#undef FALSE

#include "cps.h"
#include "parse.h"
#include "stdlib.h"

void add_stdlib(struct env *env) {
	HRSRC hResStdlib = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_STDLIB), RT_RCDATA);
	if (!hResStdlib) abort();
	DWORD stdlibLen = SizeofResource(NULL, hResStdlib);
	if (!stdlibLen) abort();
	HGLOBAL hStdlib = LoadResource(NULL, hResStdlib);
	if (!hStdlib) abort();
	const char *stdlibData = LockResource(hStdlib);

	struct buf stdlib;
	init_buf(stdlibData, stdlibLen, &stdlib);

	struct obj *obj;
	if (parse(&stdlib, &obj) != PARSE_OK) {
		abort();
	}
	while (obj != NIL) {
		run_cps(CAR(obj), env, NULL /*failed*/);
		obj = CDR(obj);
	}
}
