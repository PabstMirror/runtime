#include "SQF_inst.h"

#include <malloc.h>
#include <string.h>

inline PINST inst(DATA_TYPE dt)
{
	PINST inst = malloc(sizeof(INST));
	inst->type = dt;
	inst->data.ptr = 0;
	return inst;
}
PINST inst_nop(void)
{
	return inst(INST_NOP);
}
PINST inst_command(CPCMD cmd)
{
	PINST p = inst(INST_COMMAND);
	p->data.ptr = cmd;
	return p;
}
PINST inst_value(VALUE val)
{
	PINST p = inst(INST_VALUE);
	p->data.ptr = malloc(sizeof(VALUE));
	((PVALUE)p->data.ptr)->type = val.type;
	((PVALUE)p->data.ptr)->val = val.val;
	return p;
}
PINST inst_load_var(const char* name)
{
	PINST p = inst(INST_LOAD_VAR);
	int len = strlen(name);
	p->data.ptr = malloc(sizeof(char) * (len + 1));
	strcpy(p->data.ptr, name);
	return p;
}
PINST inst_store_var(const char* name)
{
	PINST p = inst(INST_STORE_VAR);
	int len = strlen(name);
	p->data.ptr = malloc(sizeof(char) * (len + 1));
	strcpy(p->data.ptr, name);
	return p;
}
PINST inst_store_var_local(const char* name)
{
	PINST p = inst(INST_STORE_VAR_LOCAL);
	int len = strlen(name);
	p->data.ptr = malloc(sizeof(char) * (len + 1));
	strcpy(p->data.ptr, name);
	return p;
}
PINST inst_scope(const char* name)
{
	PINST p = inst(INST_SCOPE);
	PSCOPE s = malloc(sizeof(SCOPE));
	p->data.ptr = s;
	s->name_len = strlen(name);
	s->name = malloc(sizeof(char) * (s->name_len + 1));
	strcpy(s->name, name);
	s->varstack_size = 32;
	s->varstack_top = 0;
	s->varstack_name = malloc(sizeof(char*) * s->varstack_size);
	s->varstack_value = malloc(sizeof(VALUE*) * s->varstack_size);
	return p;
}

void inst_destroy(PINST inst)
{
	switch (inst->type)
	{
	case INST_NOP:
	case INST_COMMAND:

		break;
	case INST_VALUE:
		inst_destroy_value(get_value(0, inst));
		break;
	case INST_LOAD_VAR:
		inst_destroy_var(get_var_name(0, inst));
		break;
	case INST_STORE_VAR:
		inst_destroy_var(get_var_name(0, inst));
		break;
	case INST_STORE_VAR_LOCAL:
		inst_destroy_var(get_var_name(0, inst));
		break;
	case INST_SCOPE:
		inst_destroy_scope(get_scope(0, inst));
		break;
	default:
		__asm int 3;
		break;
	}
	free(inst);
}
void inst_destroy_scope(PSCOPE scope)
{
	int i;
	free(scope->name);
	for (i = 0; i < scope->varstack_top; i++)
	{
		inst_destroy_value(scope->varstack_value[i]);
		free(scope->varstack_name[i]);
	}
	free(scope);
}
void inst_destroy_value(PVALUE val)
{
	CPCMD cmd = val->type;
	val->type = 0;
	if (cmd->callback != 0)
	{
		cmd->callback(val);
	}
	free(val);
}
void inst_destroy_var(char* name)
{
	free(name);
}

