#include "string_map.h"
#include "string_op.h"
#include "textrange.h"
#include "SQF.h"
#include "SQF_types.h"
#include "SQF_parse.h"
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef __linux
#include <alloca.h>
#endif // _GCC
#ifdef _WIN32
#include <malloc.h>
#endif // _WIN32
#include <math.h>
#ifndef M_PI
#define M_PI 3.1415926535
#endif // !M_PI 3.1415926535

#include <string.h>
#include <setjmp.h>
#include <sys/timeb.h>
#include <stdint.h>
#if _WIN32 & _DEBUG
#include <crtdbg.h>
#endif

int64_t system_time_ms()
{
	#ifdef _WIN32
	struct _timeb timebuffer;
	_ftime(&timebuffer);
	return (int64_t)(((timebuffer.time * 1000) + timebuffer.millitm));
	#else
	struct timeb timebuffer;
	ftime(&timebuffer);
	return (int64_t)(((timebuffer.time * 1000) + timebuffer.millitm));
	#endif
}

static PSTRING outputbuffer = 0;
static jmp_buf program_exit;
static const char* current_code = 0;
static PVM vm = 0;
static int64_t systime_start = 0;

void stringify_value(PVM vm, PSTRING str, PVALUE val)
{
	PARRAY arr;
	int i;
	char* strptr;
	if (val->type == STRING_TYPE())
	{
		string_modify_append(str, "\"");
		string_modify_append(str, ((PSTRING)val->val.ptr)->val);
		string_modify_append(str, "\"");
	}
	else if (val->type == SCALAR_TYPE())
	{
		strptr = alloca(sizeof(char) * 64);
		sprintf(strptr, "%lf", val->val.f);
		string_modify_append(str, strptr);
	}
	else if (val->type == BOOL_TYPE())
	{
		string_modify_append(str, val->val.i > 0 ? "true" : "false");
	}
	else if (val->type == CODE_TYPE() || val->type == WHILE_TYPE())
	{
		string_modify_append(str, "{");
		string_modify_append(str, ((PCODE)val->val.ptr)->val);
		string_modify_append(str, "}");
	}
	else if (val->type == ARRAY_TYPE())
	{
		arr = ((PARRAY)val->val.ptr);
		string_modify_append(str, "[");
		for (i = 0; i < arr->top; i++)
		{
			if (i > 0)
			{
				string_modify_append(str, ", ");
			}
			stringify_value(vm, str, arr->data[i]);
		}
		string_modify_append(str, "]");
	}
	else
	{
		string_modify_append(str, "<");
		string_modify_append(str, val->type->name);
		string_modify_append(str, ">");
	}
}

void CMD_PLUS(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	PARRAY arr;
	PSTRING str;
	int i, j;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type == SCALAR_TYPE())
	{
		if (right_val->type != SCALAR_TYPE())
		{
			vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
			inst_destroy(left);
			inst_destroy(right);
			push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
			return;
		}
		push_stack(vm, vm->stack, inst_value(value(left_val->type, base_float(left_val->val.f + right_val->val.f))));
	}
	else if (left_val->type == STRING_TYPE())
	{
		if (right_val->type != STRING_TYPE())
		{
			vm->error(ERR_RIGHT_TYPE ERR_STRING, vm->stack);
			inst_destroy(left);
			inst_destroy(right);
			push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
			return;
		}
		str = string_concat(((PSTRING)left_val->val.ptr), ((PSTRING)right_val->val.ptr));
		push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(str))));
	}
	else if (left_val->type == ARRAY_TYPE())
	{
		j = 0;
		if (right_val->type == ARRAY_TYPE())
		{
			arr = ((PARRAY)right_val->val.ptr);
			for (i = arr->top - 1; i >= 0; i--, j++)
			{
				push_stack(vm, vm->stack, inst_arr_push());
				push_stack(vm, vm->stack, inst_value(value(arr->data[i]->type, arr->data[i]->val)));
			}
		}
		else
		{
			push_stack(vm, vm->stack, inst_arr_push());
			push_stack(vm, vm->stack, inst_value(value(right_val->type, right_val->val)));
		}
		arr = ((PARRAY)left_val->val.ptr);
		for (i = arr->top - 1; i >= 0; i--)
		{
			push_stack(vm, vm->stack, inst_arr_push());
			push_stack(vm, vm->stack, inst_value(value(arr->data[i]->type, arr->data[i]->val)));
		}
		push_stack(vm, vm->stack, inst_value(value(ARRAY_TYPE(), base_voidptr(array_create2(j)))));
	}
	else
	{
		if (right_val->type != STRING_TYPE())
		{
			vm->error(ERR_ERR, vm->stack);
			inst_destroy(left);
			inst_destroy(right);
			push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
			return;
		}
	}
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_MINUS(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		return;
	}
	push_stack(vm, vm->stack, inst_value(value(left_val->type, base_float(left_val->val.f - right_val->val.f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_MINUS_UNARY(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		return;
	}
	push_stack(vm, vm->stack, inst_value(value(right_val->type, base_float(-right_val->val.f))));
	inst_destroy(right);
}
void CMD_MULTIPLY(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		return;
	}
	push_stack(vm, vm->stack, inst_value(value(left_val->type, base_float(left_val->val.f * right_val->val.f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_DIVIDE(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		return;
	}
	push_stack(vm, vm->stack, inst_value(value(left_val->type, base_float(left_val->val.f / right_val->val.f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_DIAG_LOG(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	PSTRING str = string_create(0);
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		string_destroy(str);
		inst_destroy(right);
		return;
	}
	stringify_value(vm, str, right_val);
	string_modify_append(outputbuffer, str->val);
	string_modify_append(outputbuffer, "\n");
	string_destroy(str);
	inst_destroy(right);
	push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
}
void CMD_PRIVATE(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	PSTRING str;
	PARRAY arr;
	int i;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
		return;
	}
	if (right_val->type == STRING_TYPE())
	{
		str = (PSTRING)right_val;
		if (str->length == 0)
		{
			vm->error(ERR_RIGHT ERR_NOT_EMPTY, vm->stack);
		}
		else if (str->val[0] != '_')
		{
			vm->error(ERR_SPECIAL_PRIVATE_1, vm->stack);
		}
		else
		{
			store_in_scope(vm, top_scope(vm), str->val, value(ANY_TYPE(), base_voidptr(0)));
		}
	}
	else if (right_val->type == ARRAY_TYPE())
	{
		arr = right_val->val.ptr;
		for (i = 0; i < arr->top; i++)
		{
			if (arr->data[i]->type != STRING_TYPE())
			{
				vm->error(ERR_RIGHT_TYPE ERR_STRING, vm->stack);
			}
			else
			{
				str = arr->data[i]->val.ptr;
				if (str->length == 0)
				{
					vm->error(ERR_RIGHT ERR_NOT_EMPTY, vm->stack);
				}
				else if (str->val[0] != '_')
				{
					vm->error(ERR_SPECIAL_PRIVATE_1, vm->stack);
				}
				else
				{
					store_in_scope(vm, top_scope(vm), str->val, value(ANY_TYPE(), base_voidptr(0)));
				}
			}
		}
	}
	else
	{
		vm->error(ERR_RIGHT_TYPE ERR_STRING ERR_OR ERR_ARRAY, vm->stack);
	}

	inst_destroy(right);
	push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
}

void CMD_IF(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	int flag;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
		return;
	}
	if (right_val->type == BOOL_TYPE())
	{
		flag = right_val->val.i > 0;
	}
	else if (right_val->type == SCALAR_TYPE())
	{
		flag = right_val->val.f > 0;
	}
	else
	{
		vm->error(ERR_RIGHT_TYPE ERR_BOOL ERR_OR ERR_SCALAR, vm->stack);
	}
	push_stack(vm, vm->stack, inst_value(value(IF_TYPE(), base_int(flag))));

	inst_destroy(right);
}
void CMD_THEN(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	PCODE code;
	PARRAY arr;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}

	if (left_val->type != IF_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_IF, vm->stack);
	}
	if (right_val->type == ARRAY_TYPE())
	{
		arr = right_val->val.ptr;
		if (arr->top == 0)
		{
			vm->error(ERR_RIGHT ERR_NOT_EMPTY, vm->stack);
		}
		else if (arr->data[0]->type != CODE_TYPE())
		{
			vm->error(ERR_ERR ERR_ARRAY_(0) ERR_WAS_EXPECTED ERR_OF_TYPE ERR_CODE, vm->stack);
		}
		else if (arr->top > 1 && arr->data[1]->type != CODE_TYPE())
		{
			vm->error(ERR_ERR ERR_ARRAY_(1) ERR_WAS_EXPECTED ERR_OF_TYPE ERR_CODE, vm->stack);
		}
		else
		{
			if (left_val->val.i)
			{
				code = arr->data[0]->val.ptr;
			}
			else if (arr->top > 1)
			{
				code = arr->data[1]->val.ptr;
			}
			if (code != 0)
			{
				push_stack(vm, vm->stack, inst_code_load(1));
				push_stack(vm, vm->stack, inst_value(value(CODE_TYPE(), base_voidptr(code))));
			}
		}
	}
	else if (right_val->type == CODE_TYPE())
	{
		if (left_val->val.i)
		{
			push_stack(vm, vm->stack, inst_code_load(1));
			push_stack(vm, vm->stack, inst_value(value(CODE_TYPE(), base_voidptr(right_val->val.ptr))));
		}
	}
	else
	{
		vm->error(ERR_RIGHT_TYPE ERR_CODE ERR_OR ERR_ARRAY, vm->stack);
	}

	inst_destroy(left);
	inst_destroy(right);
}
void CMD_ELSE(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}

	push_stack(vm, vm->stack, inst_arr_push());
	push_stack(vm, vm->stack, inst_value(value(CODE_TYPE(), right_val->val)));
	push_stack(vm, vm->stack, inst_arr_push());
	push_stack(vm, vm->stack, inst_value(value(CODE_TYPE(), left_val->val)));
	push_stack(vm, vm->stack, inst_value(value(ARRAY_TYPE(), base_voidptr(array_create2(2)))));

	inst_destroy(left);
	inst_destroy(right);
}
void CMD_TRUE(void* input, CPCMD self)
{
	PVM vm = input;
	push_stack(vm, vm->stack, inst_value(value(BOOL_TYPE(), base_int(1))));
}
void CMD_FALSE(void* input, CPCMD self)
{
	PVM vm = input;
	push_stack(vm, vm->stack, inst_value(value(BOOL_TYPE(), base_int(0))));
}
void CMD_HELP(void* input, CPCMD self)
{
	PVM vm = input;
	int i;
	CPCMD cmd;
	char* buffer = 0;
	unsigned int buffsize = 0, buffsize2;
	string_modify_append(outputbuffer, "ERRORS might result in crash\n\n");
	string_modify_append(outputbuffer, "NAME:TYPE:PRECEDENCE:DESCRIPTION\n");
	for (i = 0; i < vm->cmds_top; i++)
	{
		cmd = vm->cmds[i];
		buffsize2 = snprintf(0, 0, "%s:%c:%d:%s\n", cmd->name, cmd->type, cmd->precedence_level, cmd->description);
		if (buffsize2 > buffsize)
		{
			buffer = realloc(buffer, sizeof(char) * (buffsize2 + 1));
			buffsize = buffsize2;
		}
		snprintf(buffer, buffsize, "%s:%c:%d:%s\n", cmd->name, cmd->type, cmd->precedence_level, cmd->description);
		string_modify_append(outputbuffer, buffer);
	}
	free(buffer);
	push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
}

void CMD_STR(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	PSTRING str = string_create(0);
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
		return;
	}
	stringify_value(vm, str, right_val);
	inst_destroy(right);
	push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(str))));
}

void CMD_LARGETTHEN(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	push_stack(vm, vm->stack, inst_value(value(BOOL_TYPE(), base_int(left_val->val.f > right_val->val.f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_LESSTHEN(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	push_stack(vm, vm->stack, inst_value(value(BOOL_TYPE(), base_int(left_val->val.f < right_val->val.f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_LARGETTHENOREQUAL(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	push_stack(vm, vm->stack, inst_value(value(BOOL_TYPE(), base_int(left_val->val.f >= right_val->val.f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_LESSTHENOREQUAL(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	push_stack(vm, vm->stack, inst_value(value(BOOL_TYPE(), base_int(left_val->val.f <= right_val->val.f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_EQUAL(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	push_stack(vm, vm->stack, inst_value(value(BOOL_TYPE(), base_int(left_val->val.f == right_val->val.f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_ANDAND(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != BOOL_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_BOOL, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type == BOOL_TYPE())
	{
		push_stack(vm, vm->stack, inst_value(value(left_val->type, base_int(left_val->val.i && right_val->val.i))));
		inst_destroy(left);
		inst_destroy(right);
	}
	else if (right_val->type == CODE_TYPE())
	{
		if (!left_val->val.i)
		{
			push_stack(vm, vm->stack, inst_value(value(left_val->type, base_int(0))));
		}
		else
		{
			push_stack(vm, vm->stack, inst_command(self));
			push_stack(vm, vm->stack, left);
			push_stack(vm, vm->stack, inst_code_load(1));
			push_stack(vm, vm->stack, right);
		}
	}
	else
	{
		vm->error(ERR_RIGHT_TYPE ERR_BOOL, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
}
void CMD_OROR(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != BOOL_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_BOOL, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type == BOOL_TYPE())
	{
		push_stack(vm, vm->stack, inst_value(value(left_val->type, base_int(left_val->val.i || right_val->val.i))));
		inst_destroy(left);
		inst_destroy(right);
	}
	else if (right_val->type == CODE_TYPE())
	{
		if (left_val->val.i)
		{
			push_stack(vm, vm->stack, inst_value(value(left_val->type, base_int(1))));
		}
		else
		{
			push_stack(vm, vm->stack, inst_command(self));
			push_stack(vm, vm->stack, left);
			push_stack(vm, vm->stack, inst_code_load(1));
			push_stack(vm, vm->stack, right);
		}
	}
	else
	{
		vm->error(ERR_RIGHT_TYPE ERR_BOOL, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
}

void CMD_SELECT(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	PARRAY arr;
	PVALUE tmp;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	//ToDo: implement more select variants https://community.bistudio.com/wiki/select


	if (left_val->type == ARRAY_TYPE())
	{
		arr = left_val->val.ptr;
		if (right_val->type == SCALAR_TYPE())
		{
			tmp = arr->data[(int)roundf(right_val->val.f)];
			push_stack(vm, vm->stack, inst_value(value(tmp->type, tmp->val)));
		}
		else
		{
			vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		}
	}
	else
	{
		vm->error(ERR_LEFT_TYPE ERR_ARRAY, vm->stack);
	}


	inst_destroy(left);
	inst_destroy(right);
}


void CMD_WHILE(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
		return;
	}
	if (right_val->type != CODE_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_CODE, vm->stack);
		inst_destroy(right);
		return;
	}
	right_val->type = WHILE_TYPE();
	push_stack(vm, vm->stack, right);
}
void CMD_DO(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	PCODE code;
	PFOR pfor;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != CODE_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_CODE, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	else
	{
		code = right_val->val.ptr;
	}
	if (left_val->type == WHILE_TYPE())
	{
		push_stack(vm, vm->stack, inst_command(find_command(vm, "do", 'b')));
		push_stack(vm, vm->stack, left);
		push_stack(vm, vm->stack, right);
		push_stack(vm, vm->stack, inst_code_load(0));
		push_stack(vm, vm->stack, inst_value(value(CODE_TYPE(), base_voidptr(code))));
		push_stack(vm, vm->stack, inst_pop_eval(5, 0));
		push_stack(vm, vm->stack, inst_code_load(0));
		push_stack(vm, vm->stack, inst_value(value(CODE_TYPE(), left_val->val)));
		push_stack(vm, vm->stack, inst_clear_work());
	}
	else if (left_val->type == FOR_TYPE())
	{
		pfor = left_val->val.ptr;
		if (pfor->started)
		{
			pfor->current += pfor->step;
		}
		else
		{
			pfor->current = pfor->start;
			pfor->started = 1;
		}
		if (pfor->step > 0 ? pfor->current < pfor->end : pfor->current > pfor->end)
		{
			push_stack(vm, vm->stack, inst_command(find_command(vm, "do", 'b')));
			push_stack(vm, vm->stack, left);
			push_stack(vm, vm->stack, right);
			push_stack(vm, vm->stack, inst_scope("loop"));
			push_stack(vm, vm->stack, inst_code_load(0));
			push_stack(vm, vm->stack, inst_value(value(CODE_TYPE(), right_val->val)));
			push_stack(vm, vm->stack, inst_store_var_local(pfor->variable));
			push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(pfor->current))));
			push_stack(vm, vm->stack, inst_clear_work());
		}
		else
		{
			inst_destroy(left);
			inst_destroy(right);
		}
	}
	else
	{
		vm->error(ERR_LEFT_TYPE ERR_WHILE ERR_OR ERR_FOR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
}
void CMD_TYPENAME(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	PSTRING str;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	str = string_create2(right_val->type->name);
	push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(str))));
	inst_destroy(right);
}
void CMD_COUNT(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	PCOUNT count;
	PARRAY arr;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type == CODE_TYPE())
	{
		if (right_val->type != ARRAY_TYPE())
		{
			vm->error(ERR_RIGHT_TYPE ERR_ARRAY, vm->stack);
			inst_destroy(left);
			inst_destroy(right);
			return;
		}
		arr = right_val->val.ptr;
		if (arr->top == 0)
		{
			inst_destroy(left);
			inst_destroy(right);
		}
		else
		{
			count = count_create(left_val->val.ptr, right_val->val.ptr);
			push_stack(vm, vm->stack, inst_command(self));
			push_stack(vm, vm->stack, inst_value(value(COUNT_TYPE(), base_voidptr(count))));
			push_stack(vm, vm->stack, inst_scope(0));
			push_stack(vm, vm->stack, inst_code_load(0));
			push_stack(vm, vm->stack, left);
			push_stack(vm, vm->stack, inst_store_var_local("_x"));
			push_stack(vm, vm->stack, inst_value(value(arr->data[count->curtop]->type, arr->data[count->curtop]->val)));
			inst_destroy(right);
		}
	}
	else if (left_val->type == COUNT_TYPE())
	{
		if (right_val->type != BOOL_TYPE())
		{
			vm->error(ERR_RIGHT_TYPE ERR_BOOL, vm->stack);
			inst_destroy(left);
			inst_destroy(right);
			return;
		}
		count = left_val->val.ptr;
		arr = count->arr->val.ptr;
		if (right_val->val.i)
		{
			count->count++;
		}
		if (++(count->curtop) == arr->top)
		{
			push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(count->count))));
			inst_destroy(left);
			inst_destroy(right);
		}
		else
		{
			push_stack(vm, vm->stack, inst_command(self));
			push_stack(vm, vm->stack, left);
			push_stack(vm, vm->stack, inst_scope(0));
			push_stack(vm, vm->stack, inst_code_load(0));
			push_stack(vm, vm->stack, inst_value(value(CODE_TYPE(), base_voidptr(count->code->val.ptr))));
			push_stack(vm, vm->stack, inst_store_var_local("_x"));
			push_stack(vm, vm->stack, inst_value(value(arr->data[count->curtop]->type, arr->data[count->curtop]->val)));
			inst_destroy(right);
		}
	}
	else
	{
		vm->error(ERR_LEFT_TYPE ERR_CODE ERR_OR ERR_COUNT, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
	}
}
void CMD_COUNT_UNARY(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
		return;
	}

	if (right_val->type == STRING_TYPE())
	{
		push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(((PSTRING)right_val->val.ptr)->length))));
	}
	else if (right_val->type == ARRAY_TYPE())
	{
		push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(((PARRAY)right_val->val.ptr)->top))));
	}
	else
	{
		vm->error(ERR_RIGHT_TYPE ERR_STRING ERR_OR ERR_ARRAY, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
	}
	inst_destroy(right);
}
void CMD_FORMAT(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	PSTRING str;
	PSTRING str_out;
	PARRAY arr;
	char* ptr;
	char* ptr_last;
	char* endptr;
	int index;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
		return;
	}

	if (right_val->type == ARRAY_TYPE())
	{
		arr = right_val->val.ptr;
		if (arr->top == 0)
		{
			vm->error(ERR_RIGHT ERR_NOT_EMPTY, vm->stack);
			push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		}
		else if (arr->data[0]->type != STRING_TYPE())
		{
			vm->error(ERR_ERR ERR_ARRAY_(0) ERR_WAS_EXPECTED ERR_OF_TYPE ERR_STRING, vm->stack);
			push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		}
		else
		{
			str = arr->data[0]->val.ptr;
			str_out = string_create(0);
			ptr_last = str->val;
			while ((ptr = strchr(ptr_last, '%')) != 0)
			{
				string_modify_nappend(str_out, ptr_last, ptr - ptr_last);
				index = strtof(ptr + 1, &endptr);
				if (endptr == ptr)
				{
					vm->error(ERR_ERR ERR_SPECIAL_FORMAT_1, vm->stack);
				}
				else if (index >= arr->top)
				{
					vm->error(ERR_ERR ERR_SPECIAL_FORMAT_2, vm->stack);
				}
				else
				{
					stringify_value(vm, str_out, arr->data[index]);
				}
				ptr_last = endptr;
			}
			string_modify_append(str_out, ptr_last);
			push_stack(vm, vm->stack, inst_value(value(STRING_TYPE(), base_voidptr(str_out))));
		}
	}
	else
	{
		vm->error(ERR_RIGHT_TYPE ERR_ARRAY, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
	}
	inst_destroy(right);
}
void CMD_CALL(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}

	if (right_val->type == CODE_TYPE())
	{
		push_stack(vm, vm->stack, inst_scope(0));
		push_stack(vm, vm->stack, inst_code_load(0));
		push_stack(vm, vm->stack, right);
		push_stack(vm, vm->stack, inst_store_var_local("_this"));
		push_stack(vm, vm->stack, inst_value(value(left_val->type, left_val->val)));
		inst_destroy(left);
	}
	else
	{
		vm->error(ERR_RIGHT_TYPE ERR_CODE, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(right);
	}
}
void CMD_CALL_UNARY(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
		return;
	}

	if (right_val->type == CODE_TYPE())
	{
		push_stack(vm, vm->stack, inst_scope(0));
		push_stack(vm, vm->stack, inst_code_load(0));
		push_stack(vm, vm->stack, right);
		push_stack(vm, vm->stack, inst_store_var_local("_this"));
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
	}
	else
	{
		vm->error(ERR_RIGHT_TYPE ERR_CODE, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(right);
	}
}

void CMD_FOR(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	PFOR f;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != STRING_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_STRING, vm->stack);
		inst_destroy(right);
		return;
	}
	f = for_create(((PSTRING)right_val->val.ptr)->val);
	push_stack(vm, vm->stack, inst_value(value(FOR_TYPE(), base_voidptr(f))));
	inst_destroy(right);
}
void CMD_FROM(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	PFOR f;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != FOR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_FOR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	f = left_val->val.ptr;
	f->start = (int)roundf(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(FOR_TYPE(), base_voidptr(f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_TO(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	PFOR f;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != FOR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_FOR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	f = left_val->val.ptr;
	f->end = (int)roundf(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(FOR_TYPE(), base_voidptr(f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_STEP(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	PFOR f;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != FOR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_FOR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	f = left_val->val.ptr;
	f->step = right_val->val.f;
	push_stack(vm, vm->stack, inst_value(value(FOR_TYPE(), base_voidptr(f))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_PARSINGNAMESPACE(void* input, CPCMD self)
{
	PVM vm = input;
	push_stack(vm, vm->stack, inst_value(value(NAMESPACE_TYPE(), base_voidptr(sqf_parsingNamespace()))));
}
void CMD_MISSIONNAMESPACE(void* input, CPCMD self)
{
	PVM vm = input;
	push_stack(vm, vm->stack, inst_value(value(NAMESPACE_TYPE(), base_voidptr(sqf_missionNamespace()))));
}
void CMD_UINAMESPACE(void* input, CPCMD self)
{
	PVM vm = input;
	push_stack(vm, vm->stack, inst_value(value(NAMESPACE_TYPE(), base_voidptr(sqf_uiNamespace()))));
}
void CMD_PROFILENAMESPACE(void* input, CPCMD self)
{
	PVM vm = input;
	push_stack(vm, vm->stack, inst_value(value(NAMESPACE_TYPE(), base_voidptr(sqf_profileNamespace()))));
}
void CMD_DIAG_TICKTIME(void* input, CPCMD self)
{
	PVM vm = input;
	int64_t systime_cur = system_time_ms();
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(systime_cur - systime_start))));
}

//https://community.bistudio.com/wiki/Math_Commands
void CMD_ABS(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = abs((int)right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_DEG(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = right_val->val.f * (180 / M_PI);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_LOG(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = log10(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_PI(void* input, CPCMD self)
{
	PVM vm = input;
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(M_PI))));
}
void CMD_SIN(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = sin(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_COS(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = cos(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_TAN(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = tan(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_EXP(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = exp(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_ASIN(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = asin(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_ACOS(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = acos(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_ATAN(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = atan(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_RAD(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = right_val->val.f / 360 * M_PI * 2;
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_SQRT(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = sqrt(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_CEIL(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = ceil(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_RANDOM(void* input, CPCMD self)
{
	//ToDo: https://community.bistudio.com/wiki/random implement Alternative Syntax 1 & 2 & 3
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = ((double)rand() / (double)(RAND_MAX / right_val->val.f));
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_FLOOR(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = floor(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_LN(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = log(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}
void CMD_ROUND(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	float f;
	if (right_val == 0)
	{
		inst_destroy(right);
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		inst_destroy(right);
		return;
	}
	f = round(right_val->val.f);
	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(f))));
	inst_destroy(right);
}

void CMD_ATAN2(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	float l, r;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	l = left_val->val.f;
	r = right_val->val.f;

	l = atan2(l, r);

	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(l))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_MIN(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	float l, r;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	l = left_val->val.f;
	r = right_val->val.f;

	l = l < r ? l : r;

	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(l))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_MAX(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	float l, r;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	l = left_val->val.f;
	r = right_val->val.f;

	l = l > r ? l : r;

	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(l))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_MOD(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	float l, r;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	l = left_val->val.f;
	r = right_val->val.f;


	l = fmod(l, r);

	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(l))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_NOT(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
		return;
	}
	if (right_val->type == BOOL_TYPE())
	{
		push_stack(vm, vm->stack, inst_value(value(BOOL_TYPE(), base_int(right_val->val.i))));
	}
	else
	{
		vm->error(ERR_RIGHT_TYPE ERR_BOOL, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(right);
		return;
	}
}
void CMD_POWEROF(void* input, CPCMD self)
{
	PVM vm = input;
	PINST left;
	PINST right;
	PVALUE left_val;
	PVALUE right_val;
	float l, r;
	left = pop_stack(vm, vm->work);
	right = pop_stack(vm, vm->work);
	left_val = get_value(vm, vm->stack, left);
	right_val = get_value(vm, vm->stack, right);
	if (left_val == 0 || right_val == 0)
	{
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (left_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_LEFT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	if (right_val->type != SCALAR_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_SCALAR, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(left);
		inst_destroy(right);
		return;
	}
	l = left_val->val.f;
	r = right_val->val.f;


	l = pow(l, r);

	push_stack(vm, vm->stack, inst_value(value(SCALAR_TYPE(), base_float(l))));
	inst_destroy(left);
	inst_destroy(right);
}
void CMD_COMMENT(void* input, CPCMD self)
{
	PVM vm = input;
	PINST right;
	PVALUE right_val;
	float l, r;
	right = pop_stack(vm, vm->work);
	right_val = get_value(vm, vm->stack, right);
	if (right_val == 0)
	{
		inst_destroy(right);
		return;
	}
	if (right_val->type != STRING_TYPE())
	{
		vm->error(ERR_RIGHT_TYPE ERR_STRING, vm->stack);
		push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
		inst_destroy(right);
		return;
	}
	push_stack(vm, vm->stack, inst_value(value(NOTHING_TYPE(), base_int(0))));
	inst_destroy(right);
}

char* get_line(char* line, size_t lenmax)
{
	char* line_start = line;
	size_t len = lenmax;
	int c;

	if (line == NULL)
		return NULL;

	for (;;)
	{
		c = fgetc(stdin);
		if (c == EOF)
			break;

		if (--len == 0)
		{
			len++;
			line--;
		}

		if ((*line++ = c) == '\n')
			break;
	}
	*line = '\0';
	return line_start;
}

#define LINEBUFFER_SIZE 256

void custom_error(const char* errMsg, PSTACK stack)
{
	int len, i, j;
	const char* str;
	char* str2;
	PDBGINF dbginf;
	if (current_code != 0 && stack->allow_dbg)
	{
		dbginf = 0;
		for (i = stack->top - 1; i >= 0; i--)
		{
			if (stack->data[i]->type == INST_DEBUG_INFO)
			{
				dbginf = get_dbginf(0, 0, stack->data[i]);
				break;
			}
		}
		if (dbginf != 0)
		{
			i = dbginf->offset - 15;
			len = 30;
			if (i < 0)
			{
				len += i;
				i = 0;
			}
			for (j = i; j < i + len; j++)
			{
				if (current_code[j] == '\0' || current_code[j] == '\n')
				{
					if (j < dbginf->offset)
					{
						i = j + 1;
					}
					else
					{
						len = j - i;
						break;
					}
				}
			}
			str = current_code + i;
			string_modify_nappend(outputbuffer, str, len);
			string_modify_append(outputbuffer, "\n");
			for (; i < dbginf->offset; i++)
			{
				string_modify_append(outputbuffer, " ");
			}
			string_modify_append(outputbuffer, "^\n");
			len = snprintf(0, 0, "[ERR][L%d|C%d] %s\n", dbginf->line, dbginf->col, errMsg);
			str2 = alloca(sizeof(char) * (len + 1));
			snprintf(str2, len + 1, "[ERR][L%d|C%d] %s\n", dbginf->line, dbginf->col, errMsg);
			str2[len] = '\0';
			string_modify_append(outputbuffer, str2);
		}
		else
		{
			goto normal;
		}
	}
	else
	{
normal:
		len = snprintf(0, 0, "[ERR] %s\n", errMsg);
		str2 = alloca(sizeof(char) * (len + 1));
		snprintf(str2, len + 1, "[ERR] %s\n", errMsg);
		str2[len] = '\0';
		string_modify_append(outputbuffer, str2);
	}
	vm->die_flag = 1;
	//longjmp(program_exit, 1);
}

#ifdef _WIN32
__declspec(dllexport) const char* start_program(const char* input, unsigned char* success)
{
#else
__attribute__((visibility("default"))) const char* start_program(const char* input, unsigned char* success)
{
#endif
	int val;
	int i;
	vm = sqfvm(1000, 50, 100, 1);
	vm->error = custom_error;
	if (outputbuffer == 0)
	{
		outputbuffer = string_create(0);
	}
	else
	{
		free(outputbuffer->val);
		outputbuffer->val = 0;
		outputbuffer->length = 0;
	}
	if (systime_start == 0)
	{
		systime_start = system_time_ms();
	}
	/*
	//register_command(vm, create_command("SCALAR", 't', 0, 0));
	//register_command(vm, create_command("BOOL", 't', 0, 0));
	//register_command(vm, create_command("ARRAY", 't', 0, 0));
	//register_command(vm, create_command("STRING", 't', 0, 0));
	//register_command(vm, create_command("NOTHING", 't', 0, 0));
	register_command(vm, create_command("ANY", 't', 0, 0));
	//register_command(vm, create_command("NAMESPACE", 't', 0, 0));
	register_command(vm, create_command("NaN", 't', 0, 0));
	//register_command(vm, create_command("IF", 't', 0, 0));
	//register_command(vm, create_command("WHILE", 't', 0, 0));
	/register_command(vm, create_command("FOR", 't', 0, 0));
	register_command(vm, create_command("SWITCH", 't', 0, 0));
	register_command(vm, create_command("EXCEPTION", 't', 0, 0));
	register_command(vm, create_command("WITH", 't', 0, 0));
	//register_command(vm, create_command("CODE", 't', 0, 0));
	register_command(vm, create_command("OBJECT", 't', 0, 0));
	register_command(vm, create_command("VECTOR", 't', 0, 0));
	register_command(vm, create_command("TRANS", 't', 0, 0));
	register_command(vm, create_command("ORIENT", 't', 0, 0));
	register_command(vm, create_command("SIDE", 't', 0, 0));
	register_command(vm, create_command("GROUP", 't', 0, 0));
	register_command(vm, create_command("TEXT", 't', 0, 0));
	register_command(vm, create_command("SCRIPT", 't', 0, 0));
	register_command(vm, create_command("TARGET", 't', 0, 0));
	register_command(vm, create_command("JCLASS", 't', 0, 0));
	register_command(vm, create_command("CONFIG", 't', 0, 0));
	register_command(vm, create_command("DISPLAY", 't', 0, 0));
	register_command(vm, create_command("CONTROL", 't', 0, 0));
	register_command(vm, create_command("NetObject", 't', 0, 0));
	register_command(vm, create_command("SUBGROUP", 't', 0, 0));
	register_command(vm, create_command("TEAM_MEMBER", 't', 0, 0));
	register_command(vm, create_command("TASK", 't', 0, 0));
	register_command(vm, create_command("DIARY_RECORD", 't', 0, 0));
	register_command(vm, create_command("LOCATION", 't', 0, 0));
	*/

	register_command(vm, create_command("+", 'b', CMD_PLUS, 8, "<SCALAR> + <SCALAR> | <STRING> + <STRING> | <ARRAY> + <ANY>"));
	register_command(vm, create_command("-", 'b', CMD_MINUS, 8, "<SCALAR> - <SCALAR> "));
	register_command(vm, create_command("*", 'b', CMD_MULTIPLY, 9, "<SCALAR> * <SCALAR>"));
	register_command(vm, create_command("/", 'b', CMD_DIVIDE, 9, "<SCALAR> / <SCALAR>"));
	register_command(vm, create_command(">", 'b', CMD_LARGETTHEN, 7, "<SCALAR> > <SCALAR>"));
	register_command(vm, create_command("<", 'b', CMD_LESSTHEN, 7, "<SCALAR> < <SCALAR>"));
	register_command(vm, create_command(">=", 'b', CMD_LARGETTHENOREQUAL, 7, "<SCALAR> >= <SCALAR>"));
	register_command(vm, create_command("<=", 'b', CMD_LESSTHENOREQUAL, 7, "<SCALAR> <= <SCALAR>"));
	register_command(vm, create_command("==", 'b', CMD_EQUAL, 7, "<SCALAR> > <SCALAR>"));
	register_command(vm, create_command("||", 'b', CMD_OROR, 5, "<BOOL> || <BOOL> | <BOOL> || <CODE>"));
	register_command(vm, create_command("&&", 'b', CMD_ANDAND, 6, "<BOOL> && <BOOL> | <BOOL> && <CODE>"));
	register_command(vm, create_command("or", 'b', CMD_OROR, 5, "<BOOL> or <BOOL> | <BOOL> && <CODE>"));
	register_command(vm, create_command("and", 'b', CMD_ANDAND, 6, "<BOOL> and <BOOL>"));
	register_command(vm, create_command("select", 'b', CMD_SELECT, 10, "<ARRAY> select <SCALAR>"));
	register_command(vm, create_command("then", 'b', CMD_THEN, 5, "<IF> then <ARRAY>"));
	register_command(vm, create_command("else", 'b', CMD_ELSE, 6, "<CODE> else <CODE>"));
	register_command(vm, create_command("do", 'b', CMD_DO, 0, "<WHILE> do <CODE> | <FOR> do <CODE>"));
	register_command(vm, create_command("from", 'b', CMD_FROM, 0, "<FOR> from <SCALAR>"));
	register_command(vm, create_command("to", 'b', CMD_TO, 0, "<FOR> to <SCALAR>"));
	register_command(vm, create_command("step", 'b', CMD_STEP, 0, "<FOR> step <SCALAR>"));
	register_command(vm, create_command("count", 'b', CMD_COUNT, 0, "<CODE> count <ARRAY> | <COUNT> count <BOOL>"));
	register_command(vm, create_command("call", 'b', CMD_CALL, 0, "<ANY> call <CODE>"));
	register_command(vm, create_command("atan2", 'b', CMD_ATAN2, 0, "<SCALAR> atan2 <SCALAR>"));
	register_command(vm, create_command("min", 'b', CMD_MIN, 0, "<SCALAR> min <SCALAR>"));
	register_command(vm, create_command("max", 'b', CMD_MAX, 0, "<SCALAR> max <SCALAR>"));
	register_command(vm, create_command("mod", 'b', CMD_MOD, 0, "<SCALAR> mod <SCALAR>"));
	register_command(vm, create_command("%", 'b', CMD_MOD, 0, "<SCALAR> % <SCALAR>"));
	register_command(vm, create_command("^", 'b', CMD_POWEROF, 0, "<SCALAR> ^ <SCALAR>"));

	register_command(vm, create_command("diag_log", 'u', CMD_DIAG_LOG, 0, "diag_log <ANY>"));
	register_command(vm, create_command("private", 'u', CMD_PRIVATE, 0, "private <STRING> | private <ARRAY>"));
	register_command(vm, create_command("if", 'u', CMD_IF, 0, "if <BOOL>"));
	register_command(vm, create_command("str", 'u', CMD_STR, 0, "str <ANY>"));
	register_command(vm, create_command("while", 'u', CMD_WHILE, 0, "while <CODE>"));
	register_command(vm, create_command("typeName", 'u', CMD_TYPENAME, 0, "typeName <ANY>"));
	register_command(vm, create_command("for", 'u', CMD_FOR, 0, "for <STRING>"));
	register_command(vm, create_command("-", 'u', CMD_MINUS_UNARY, 0, "- <SCALAR>"));
	register_command(vm, create_command("count", 'u', CMD_COUNT_UNARY, 0, "count <STRING> | count <ARRAY>"));
	register_command(vm, create_command("format", 'u', CMD_FORMAT, 0, "format <ARRAY>"));
	register_command(vm, create_command("call", 'u', CMD_CALL_UNARY, 0, "call <CODE>"));
	register_command(vm, create_command("abs", 'u', CMD_ABS, 0, "abs <SCALAR>"));
	register_command(vm, create_command("deg", 'u', CMD_DEG, 0, "deg <SCALAR>"));
	register_command(vm, create_command("log", 'u', CMD_LOG, 0, "log <SCALAR>"));
	register_command(vm, create_command("sin", 'u', CMD_SIN, 0, "sin <SCALAR>"));
	register_command(vm, create_command("cos", 'u', CMD_COS, 0, "cos <SCALAR>"));
	register_command(vm, create_command("tan", 'u', CMD_TAN, 0, "tan <SCALAR>"));
	register_command(vm, create_command("exp", 'u', CMD_EXP, 0, "exp <SCALAR>"));
	register_command(vm, create_command("asin", 'u', CMD_ASIN, 0, "asin <SCALAR>"));
	register_command(vm, create_command("acos", 'u', CMD_ACOS, 0, "acos <SCALAR>"));
	register_command(vm, create_command("atan", 'u', CMD_ATAN, 0, "atan <SCALAR>"));
	register_command(vm, create_command("atg", 'u', CMD_ATAN, 0, "atg <SCALAR>"));
	register_command(vm, create_command("rad", 'u', CMD_RAD, 0, "rad <SCALAR>"));
	register_command(vm, create_command("sqrt", 'u', CMD_SQRT, 0, "sqrt <SCALAR>"));
	register_command(vm, create_command("ceil", 'u', CMD_CEIL, 0, "ceil <SCALAR>"));
	register_command(vm, create_command("random", 'u', CMD_RANDOM, 0, "random <SCALAR>"));
	register_command(vm, create_command("floor", 'u', CMD_FLOOR, 0, "floor <SCALAR>"));
	register_command(vm, create_command("ln", 'u', CMD_LN, 0, "ln <SCALAR>"));
	register_command(vm, create_command("round", 'u', CMD_ROUND, 0, "round <SCALAR>"));
	register_command(vm, create_command("!", 'u', CMD_NOT, 0, "! <BOOL>"));
	register_command(vm, create_command("comment", 'u', CMD_NOT, 0, "comment <BOOL>"));


	register_command(vm, create_command("true", 'n', CMD_TRUE, 0, "true"));
	register_command(vm, create_command("false", 'n', CMD_FALSE, 0, "false"));
	register_command(vm, create_command("parsingNamespace", 'n', CMD_PARSINGNAMESPACE, 0, "parsingNamespace"));
	register_command(vm, create_command("missionNamespace", 'n', CMD_MISSIONNAMESPACE, 0, "missionNamespace"));
	register_command(vm, create_command("uiNamespace", 'n', CMD_UINAMESPACE, 0, "uiNamespace"));
	register_command(vm, create_command("profileNamespace", 'n', CMD_PROFILENAMESPACE, 0, "profileNamespace"));
	register_command(vm, create_command("diag_tickTime", 'n', CMD_DIAG_TICKTIME , 0, "diag_tickTime"));
	register_command(vm, create_command("pi", 'n', CMD_PI, 0, "pi"));




	current_code = input;
	register_command(vm, create_command("help", 'n', CMD_HELP, 0, "Displays this help text."));
	val = setjmp(program_exit);
	if (!val)
	{
		push_stack(vm, vm->stack, inst_scope("all"));
		parse(vm, input, 1);
		execute(vm);
		if(success != 0)
			*success = vm->die_flag;
	}
	if (vm->work->top != 0)
	{
		for (i = 0; i < vm->work->top; i++)
		{
			if (vm->work->data[i]->type == INST_VALUE)
			{
				if (get_value(vm, vm->stack, vm->work->data[i])->type == NOTHING_TYPE())
					continue;
				string_modify_append(outputbuffer, "[WORK]: ");
				stringify_value(vm, outputbuffer, get_value(vm, vm->stack, vm->work->data[i]));
				string_modify_append(outputbuffer, "\n");
			}
		}
	}
	destroy_sqfvm(vm);
	vm = 0;
	current_code = 0;
	return outputbuffer->val;
}


#define RETCDE_OK 0
#define RETCDE_ERROR 1
#define RETCDE_RUNTIME_ERROR 2

int load_file(PSTRING buffer, const char* fpath)
{
	FILE* fptr = fopen(fpath, "r");
	size_t size;
	size_t curlen = buffer->length;
	int tailing = 0;
	int lcount = 1;
	if (fptr == 0)
	{
		printf("[ERR] Could not open file '%s'", fpath);
		return -1;
	}
	fseek(fptr, 0, SEEK_END);
	size = ftell(fptr);;
	rewind(fptr);
	string_modify_append2(buffer, size);
	memset(buffer->val + curlen, 0, sizeof(char) * size);
	fread(buffer->val + curlen, sizeof(char), size, fptr);
	for (; curlen < buffer->length; curlen++)
	{
		if (buffer->val[curlen] == '\n')
			lcount++;
		else if (buffer->val[curlen] == '\0')
			tailing++;
	}
	if (tailing > 0)
	{
		string_modify_append2(buffer, -tailing);
	}
	return lcount;
}

int main(int argc, char** argv)
{
	char linebuffer[LINEBUFFER_SIZE];
	const char* ptr;
	int i, j, k;
	unsigned char just_execute = 0;
	unsigned char prog_success = 0;
	PVM vm;
	PSTRING pstr;
	pstr = string_create(0);
	//_CrtSetBreakAlloc(593);
	j = 0;
	for (i = 0; i < argc; i++)
	{
		ptr = argv[i];
		if (ptr[0] == '-')
		{
			switch (ptr[1])
			{
				case '\0':
					printf("[ERR] empty argument passed.");
					return RETCDE_ERROR;
				case '?':
					printf("SQF-VM Help page\n");
					printf("./prog [-j] [-i <FILE>] [-I <CODE>]\n");
					printf("\t-?\tOutputs this help\n");
					printf("\t-i\tLoads provided input file into the code-buffer.\n");
					printf("\t-I\tLoads provided input SQF code into the code-buffer.\n");
					printf("\t-a\tDisables user input and just executes the code-buffer.\n");
					return RETCDE_OK;
					break;
				case 'i':
					if (i + 1 < argc)
					{
						k = load_file(pstr, argv[++i]);
						if (k < 0)
							return RETCDE_ERROR;
						j += k;
					}
					else
					{
						printf("[ERR] -i empty parameter");
						return RETCDE_ERROR;
					}
					break;
				case 'I':
					if (i + 1 < argc)
					{
						string_modify_append(pstr, argv[++i]);
						j++;
					}
					else
					{
						printf("[ERR] -I empty parameter");
						return RETCDE_ERROR;
					}
					break;
				case 'a':
					just_execute = 1;
					break;
				default:
					break;
			}
		}
	}
	ptr = 0;
	i = j;

	if (!just_execute)
	{
		printf("Please enter your SQF code.\nTo get the capabilities, use the `help` instruction.\nTo run the code, Press <ENTER> twice.\n");
		printf("%d:\t", i++);
		while (get_line(linebuffer, LINEBUFFER_SIZE)[0] != '\n')
		{
			string_modify_append(pstr, linebuffer);
			printf("%d:\t", i++);
		}
		//string_modify_append(pstr, "diag_log str [1, 2, \"test\", [1, 2, 3]]");
	}
	if (pstr->length > 0)
		ptr = start_program(pstr->val, &prog_success);
	if (just_execute)
	{
		if (ptr != 0)
		{
			printf("%s", ptr);
		}
	}
	else
	{
		printf("-------------------------------------\n");
		if (ptr == 0)
		{
			printf("<EMPTY>\n");
		}
		else
		{
			printf("%s", ptr);
		}
		printf("-------------------------------------\n");
		printf("Press <ENTER> to finish.");
		get_line(linebuffer, LINEBUFFER_SIZE);
	}
	string_destroy(pstr);

        if (outputbuffer != 0)
		string_destroy(outputbuffer);
	namespace_destroy(sqf_missionNamespace());
	namespace_destroy(sqf_parsingNamespace());
	namespace_destroy(sqf_profileNamespace());
	namespace_destroy(sqf_uiNamespace());
	vm = sqfvm(0, 0, 0, 0);
	for (i = 0; i < vm->cmds_top; i++)
	{
		destroy_command(vm->cmds[i]);
	}
	destroy_sqfvm(vm);

	#if _WIN32 & _DEBUG
	_CrtDumpMemoryLeaks();
	#endif
	return prog_success ? RETCDE_OK : RETCDE_RUNTIME_ERROR;
}
