#include "string_map.h"
#include "SQF.h"
#include "SQF_types.h"

#include <malloc.h>
#include <string.h>

void TYPE_CODE_CALLBACK(void* input, CPCMD self);
PCMD SCALAR_TYPE(void)
{
	static PCMD cmd = 0;
	if (cmd == 0)
	{
		cmd = create_command("SCALAR", 't', 0, 0, NULL);
	}
	return cmd;
}
PCMD BOOL_TYPE(void)
{
	static PCMD cmd = 0;
	if (cmd == 0)
	{
		cmd = create_command("BOOL", 't', 0, 0, NULL);
	}
	return cmd;
}
PCMD IF_TYPE(void)
{
	static PCMD cmd = 0;
	if (cmd == 0)
	{
		cmd = create_command("IF", 't', 0, 0, NULL);
	}
	return cmd;
}
PCMD WHILE_TYPE(void)
{
	static PCMD cmd = 0;
	if (cmd == 0)
	{
		cmd = create_command("WHILE", 't', TYPE_CODE_CALLBACK, 0, NULL);
	}
	return cmd;
}
PCMD NOTHING_TYPE(void)
{
	static PCMD cmd = 0;
	if (cmd == 0)
	{
		cmd = create_command("NOTHING", 't', 0, 0, NULL);
	}
	return cmd;
}





void TYPE_CODE_CALLBACK(void* input, CPCMD self)
{
	PVALUE val = input;
	PCODE code = val->val.ptr;
	if (val->type == 0)
	{
		code->refcount--;
		if (code->refcount <= 0)
		{
			code_destroy(code);
		}
	}
	else
	{
		code->refcount++;
	}
}
PCMD CODE_TYPE(void)
{
	static PCMD cmd = 0;
	if (cmd == 0)
	{
		cmd = create_command("CODE", 't', TYPE_CODE_CALLBACK, 0, NULL);
	}
	return cmd;
}
PCODE code_create(const char* txt, int offset, int len)
{
	PCODE code = malloc(sizeof(CODE));
	code->length = len;
	code->val = malloc(sizeof(char) * (len + 1));
	strncpy(code->val, txt + offset, len);
	code->val[len] = '\0';
	code->refcount = 0;
	return code;
}
void code_destroy(PCODE code)
{
	free(code->val);
	free(code);
}


void TYPE_STRING_CALLBACK(void* input, CPCMD self)
{
	PVALUE val = input;
	PSTRING string = val->val.ptr;
	if (val->type == 0)
	{
		string->refcount--;
		if (string->refcount <= 0)
		{
			string_destroy(string);
		}
	}
	else
	{
		string->refcount++;
	}
}
PCMD STRING_TYPE(void)
{
	static PCMD cmd = 0;
	if (cmd == 0)
	{
		cmd = create_command("STRING", 't', TYPE_STRING_CALLBACK, 0, NULL);
	}
	return cmd;
}
PSTRING string_create(unsigned int len)
{
	PSTRING string = malloc(sizeof(STRING));
	string->length = len;
	string->val = 0;
	string->refcount = 0;
	if (len > 0)
	{
		string->val = malloc(sizeof(char) * (len + 1));
		memset(string->val, 0, sizeof(char) * (len + 1));
	}
	return string;
}
PSTRING string_create2(const char* str)
{
	int len = strlen(str);
	PSTRING string = string_create(len);
	if (len > 0)
	{
		strcpy(string->val, str);
	}
	return string;
}
void string_destroy(PSTRING string)
{
	if (string->val != 0)
	{
		free(string->val);
	}
	free(string);
}
PSTRING string_concat(const PSTRING l, const PSTRING r)
{
	PSTRING string = string_create(l->length + r->length);
	strcpy(string->val, l->val);
	strcpy(string->val + l->length, r->val);
	return string;
}
PSTRING string_substring(const PSTRING string, unsigned int start, int length)
{
	if (length < 0)
	{
		length = string->length - start;
	}
	PSTRING str = string_create(length);
	strncpy(str->val, string->val + start, length);
	return str;
}
void string_modify_append(PSTRING string, const char* append)
{
	if (append == 0)
		return;
	unsigned int len = strlen(append);
	char* ptr = realloc(string->val, sizeof(char) * (string->length + len + 1));
	if (ptr == 0)
		error("failed", 0);
	string->val = ptr;
	strcpy(string->val + string->length, append);
	string->length += len;
	string->val[string->length] = '\0';
}


void TYPE_ARRAY_CALLBACK(void* input, CPCMD self)
{
	PVALUE val = input;
	PARRAY arr = val->val.ptr;
	if (val->type == 0)
	{
		arr->refcount--;
		if (arr->refcount <= 0)
		{
			array_destroy(arr);
		}
	}
	else
	{
		arr->refcount++;
	}
}
PCMD ARRAY_TYPE(void)
{
	static PCMD cmd = 0;
	if (cmd == 0)
	{
		cmd = create_command("ARRAY", 't', TYPE_ARRAY_CALLBACK, 0, NULL);
	}
	return cmd;
}
PARRAY array_create2(unsigned int initialsize)
{
	PARRAY arr = malloc(sizeof(ARRAY));
	arr->data = malloc(sizeof(PVALUE) * initialsize);
	arr->size = initialsize;
	arr->top = 0;
	arr->refcount = 0;
	return arr;
}
PARRAY array_create(void)
{
	return array_create2(ARRAY_DEFAULT_INC);
}
void array_destroy(PARRAY arr)
{
	int i;
	for (i = 0; i < arr->top; i++)
	{
		inst_destroy_value(arr->data[i]);
	}
	free(arr->data);
	free(arr);
}
void array_resize(PARRAY arr, unsigned int newsize)
{
	PVALUE* data = realloc(arr->data, sizeof(PVALUE) * newsize);
	if (data != 0)
	{
		arr->data = data;
		arr->size = newsize;
	}
}
void array_push(PARRAY arr, VALUE val)
{
	if (arr->top >= arr->size)
	{
		array_resize(arr, arr->size + ARRAY_DEFAULT_INC);
	}
	arr->data[arr->top] = malloc(sizeof(VALUE));
	arr->data[arr->top]->type = val.type;
	arr->data[arr->top]->val = val.val;
	arr->top++;
}







void TYPE_FOR_CALLBACK(void* input, CPCMD self)
{
	PVALUE val = input;
	PFOR f = val->val.ptr;
	if (val->type == 0)
	{
		f->refcount--;
		if (f->refcount <= 0)
		{
			for_destroy(f);
		}
	}
	else
	{
		f->refcount++;
	}
}
PCMD FOR_TYPE(void)
{
	static PCMD cmd = 0;
	if (cmd == 0)
	{
		cmd = create_command("FOR", 't', TYPE_FOR_CALLBACK, 0, NULL);
	}
	return cmd;
}
PFOR for_create(const char* varname)
{
	PFOR f = malloc(sizeof(FOR));
	int len = strlen(varname);
	f->variable = malloc(sizeof(char) * (len + 1));
	strcpy(f->variable, varname);
	f->variable[len] = '\0';
	f->variable_length = len;
	f->start = 0;
	f->end = 0;
	f->step = 1;
	f->started = 0;
	f->refcount = 0;

	return f;
}
void for_destroy(PFOR f)
{
	if (f->variable != 0)
	{
		free(f->variable);
	}
	free(f);
}


void TYPE_NAMESPACE_CALLBACK(void* input, CPCMD self)
{
	PVALUE val = input;
	PNAMESPACE namespace = val->val.ptr;
	if (val->type == 0)
	{
		namespace->refcount--;
		if (namespace->refcount <= 0)
		{
			namespace_destroy(namespace);
		}
	}
	else
	{
		namespace->refcount++;
	}
}
PCMD NAMESPACE_TYPE(void)
{
	static PCMD cmd = 0;
	if (cmd == 0)
	{
		cmd = create_command("NAMESPACE", 't', TYPE_NAMESPACE_CALLBACK, 0, NULL);
	}
	return cmd;
}
PNAMESPACE namespace_create(void)
{
	PNAMESPACE namespace = malloc(sizeof(NAMESPACE));
	namespace->refcount = 0;
	namespace->data = sm_create_list(74, 10, 10);
	return namespace;
}
void NAMESPACE_SM_LIST_DESTROY(void* ptr)
{
	PVALUE val = ptr;
	inst_destroy_value(val);
}
void namespace_destroy(PNAMESPACE namespace)
{
	sm_destroy_list(namespace->data, NAMESPACE_SM_LIST_DESTROY);
	free(namespace);
}
void namespace_set_var(PNAMESPACE namespace, const char* var, VALUE val)
{
	PVALUE value = malloc(sizeof(VALUE));
	value->type = val.type;
	value->val = val.val;
	value = sm_set_value(namespace->data, var, value);
	if (value != 0)
	{
		NAMESPACE_SM_LIST_DESTROY(value);
	}
}
PVALUE namespace_get_var(PNAMESPACE namespace, const char* var)
{
	return sm_get_value(namespace->data, var);
}


PNAMESPACE sqf_missionNamespace(void)
{
	static PNAMESPACE ns = 0;
	if (ns == 0)
	{
		ns = namespace_create();
	}
	return ns;
}
PNAMESPACE sqf_uiNamespace(void)
{
	static PNAMESPACE ns = 0;
	if (ns == 0)
	{
		ns = namespace_create();
	}
	return ns;
}
PNAMESPACE sqf_profileNamespace(void)
{
	static PNAMESPACE ns = 0;
	if (ns == 0)
	{
		ns = namespace_create();
	}
	return ns;
}
PNAMESPACE sqf_parsingNamespace(void)
{
	static PNAMESPACE ns = 0;
	if (ns == 0)
	{
		ns = namespace_create();
	}
	return ns;
}