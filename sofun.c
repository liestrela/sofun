#include <dlfcn.h>
#include <err.h>
#include <ffi.h>
#include <stdint.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"

#define LIST_RET_FORMATS(f) \
	f("uint64_t", &ffi_type_uint64, printf("0x%016x (%lu)\n", *(uint64_t *)x, *(uint64_t *)x)) \
	f("int64_t", &ffi_type_sint64, printf("0x%016x (%ld)\n", *(int64_t *)x, *(int64_t *)x)) \
	f("uint32_t", &ffi_type_uint32, printf("0x%08x (%u)\n", *(uint32_t *)x)) \
	f("int32_t", &ffi_type_sint32, printf("0x%08x (%d)\n", *(int32_t *)x, *(int32_t *)x)) \
	f("uint16_t", &ffi_type_uint16, printf("0x%04x (%u)\n", *(uint16_t *)x)) \
	f("int16_t", &ffi_type_sint16, printf("0x%04x (%d)\n", *(int16_t *)x, *(int16_t *)x)) \
	f("uint8_t", &ffi_type_uint8,  printf("0x%02x (%u)\n", *(uint8_t *)x)) \
	f("int8_t", &ffi_type_sint8,  printf("0x%02x (%d)\n", *(int8_t *)x, *(int8_t *)x)) \
	f("f32", &ffi_type_float,  printf("%.13f\n", *(float *)x)) \
	f("f64", &ffi_type_double, printf("%.13f\n", *(double *)x)) \
	f("str", &ffi_type_pointer, printf("%p (\"%s\")\n", *(void **)x, *(char **)x)) \
	f("ptr", &ffi_type_pointer, printf("%p\n", *(void **)x))

typedef struct {
	ffi_type **data;
	size_t size;
	size_t max;
} type_l;

typedef struct {
	void **data;
	size_t size;
	size_t max;
} value_l;

#define l_pushback(l, i)\
	do {\
		if ((l)->max <= (l)->size) {\
			if ((l)->max == 0) (l)->max = 8;\
			(l)->max *= 2;\
			(l)->data = realloc((l)->data, sizeof(*(l)->data)*(l)->max);\
		}\
		(l)->data[(l)->size++] = i;\
	} while (0)\

#define l_clear(l) (l)->size = 0

int
main(int argc, char *argv[])
{
	char *exec_name = argv[0], *so_path;
	void *handle, *fun_ptr, *x;
	char line[4096] = {0}, l_storage[4096] = {0};
	stb_lexer l = {0};
	ffi_cif cif = {0};
	type_l ts = {0};
	value_l vs = {0};
	ffi_type *ret_type = &ffi_type_void;

	if (argc<2) errx(EXIT_FAILURE, "missing shared object file");

	so_path = argv[1];
	handle = dlopen(so_path, RTLD_NOW);
	if (!handle) errx(EXIT_FAILURE, "%s", dlerror());

	#ifdef DEBUG
	printf("opened %s shared object at %p\n", so_path, handle);
	#endif

	for (;;) {
		loop:
		printf("> ");

		size_t line_size = sizeof(line);
		if (!fgets(line, line_size, stdin)) break;

		stb_c_lexer_init(&l, line, line+line_size, l_storage, sizeof(l_storage));

		/* symbol name */
		if (!stb_c_lexer_get_token(&l) || l.token!=CLEX_id) continue;
		if (!strcmp(l.string, "help")) {
			warnx("type function names and parameters separated by spaces to call\n"
			      "commands available: rt (set return type), exit");
			goto loop;
		}

		if (!strcmp(l.string, "exit")) break;

		if (!strcmp(l.string, "rt")) {
			stb_c_lexer_get_token(&l);
			if (!strcmp(l.string, "rt")) {
				warnx("rt command usage: rt (void, uintn_t, intn_t, f32, f64, str, ptr)");
				goto loop;
			} else {
				if (!strcmp(l.string, "void")) ret_type = &ffi_type_void;
				#define f(label, type, fmt) \
					if (!strcmp(l.string, label)) { ret_type = type; } else
				LIST_RET_FORMATS(f) {
					warnx("invalid return type");
					goto loop;
				}
				#undef f
			}
			goto loop;
		}

		if (!(fun_ptr = dlsym(handle, l.string))) {
			warnx("symbol \"%s\" not found", l.string);
			goto loop;
		}

		#ifdef DEBUG
		if (fun_ptr) printf("\"%s\" at %p\n", l.string, fun_ptr);
		#endif

		for (size_t i=0; i<vs.size; i++)
			if (vs.data[i]) free(vs.data[i]);

		l_clear(&vs);
		l_clear(&ts);

		while (stb_c_lexer_get_token(&l)) {
			if (!l.token) break;
			switch (l.token) {
			case CLEX_intlit:
				l_pushback(&ts, &ffi_type_uint32);
				int *i = malloc(sizeof(int));
				*i = l.int_number;
				l_pushback(&vs, i);
				break;

			case CLEX_dqstring:
				l_pushback(&ts, &ffi_type_pointer);
				char **s = malloc(sizeof(char *));
				*s = strdup(l.string);
				l_pushback(&vs, s);
				break;
		
			case CLEX_floatlit:
				l_pushback(&ts, &ffi_type_float);
				float *f = malloc(sizeof(float));
				*f = l.real_number;
				l_pushback(&vs, f);
				break;

			default:
				warnx("token type not implemented");
				goto loop;
			}
		}

		if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, ts.size, ret_type, ts.data) != FFI_OK) {
			warnx("unmatched signature or abi");
			goto loop;
		}

		x = malloc(8);
		ffi_call(&cif, fun_ptr, x, vs.data);

		if (ret_type != &ffi_type_void) {
			printf("ans = ");
			#define f(label, type, fmt) if (ret_type == type) { fmt; } else
			LIST_RET_FORMATS(f) {}
			#undef f
		}
		free(x);
	}

	return EXIT_SUCCESS;
}
