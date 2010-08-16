/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ir_print_glsl_visitor.h"
#include "ir_visitor.h"
#include "glsl_types.h"
#include "glsl_parser_extras.h"
#include "ir_unused_structs.h"

static char* print_type(char* buffer, const glsl_type *t, bool arraySize);
static char* print_type_post(char* buffer, const glsl_type *t, bool arraySize);


class ir_print_glsl_visitor : public ir_visitor {
public:
	ir_print_glsl_visitor(char* buf, PrintGlslMode mode_)
	{
		indentation = 0;
		buffer = buf;
		mode = mode_;
		writeMask = ~0;
	}

	virtual ~ir_print_glsl_visitor()
	{
		/* empty */
	}

	void indent(void);

	virtual void visit(ir_variable *);
	virtual void visit(ir_function_signature *);
	virtual void visit(ir_function *);
	virtual void visit(ir_expression *);
	virtual void visit(ir_texture *);
	virtual void visit(ir_swizzle *);
	virtual void visit(ir_dereference_variable *);
	virtual void visit(ir_dereference_array *);
	virtual void visit(ir_dereference_record *);
	virtual void visit(ir_assignment *);
	virtual void visit(ir_constant *);
	virtual void visit(ir_call *);
	virtual void visit(ir_return *);
	virtual void visit(ir_discard *);
	virtual void visit(ir_if *);
	virtual void visit(ir_loop *);
	virtual void visit(ir_loop_jump *);

	int indentation;
	char* buffer;
	PrintGlslMode mode;
	unsigned writeMask;
};


char*
_mesa_print_ir_glsl(exec_list *instructions,
	    struct _mesa_glsl_parse_state *state,
		char* buffer, PrintGlslMode mode)
{
   if (state) {
	   ir_struct_usage_visitor v;
	   v.run (instructions);

      for (unsigned i = 0; i < state->num_user_structures; i++) {
	 const glsl_type *const s = state->user_structures[i];

	 if (!v.has_struct_entry(s))
		 continue;

	 buffer = talloc_asprintf_append(buffer, "struct %s {\n",
		s->name);

	 for (unsigned j = 0; j < s->length; j++) {
	    buffer = talloc_asprintf_append(buffer, "  ");
	    buffer = print_type(buffer, s->fields.structure[j].type, false);
	    buffer = talloc_asprintf_append(buffer, " %s", s->fields.structure[j].name);
        buffer = print_type_post(buffer, s->fields.structure[j].type, false);
        buffer = talloc_asprintf_append(buffer, ";\n");
	 }

	 buffer = talloc_asprintf_append(buffer, "};\n");
      }
   }

   foreach_iter(exec_list_iterator, iter, *instructions) {
      ir_instruction *ir = (ir_instruction *)iter.get();
	  if (ir->ir_type == ir_type_variable) {
		ir_variable *var = static_cast<ir_variable*>(ir);
		if (strstr(var->name, "gl_") == var->name)
			continue;
	  }

	  ir_print_glsl_visitor v (buffer, mode);
	  ir->accept(&v);
	  buffer = v.buffer;
      if (ir->ir_type != ir_type_function)
		buffer = talloc_asprintf_append(buffer, ";\n");
   }

   return buffer;
}


void ir_print_glsl_visitor::indent(void)
{
   for (int i = 0; i < indentation; i++)
      buffer = talloc_asprintf_append(buffer, "  ");
}

static char*
print_type(char* buffer, const glsl_type *t, bool arraySize)
{
   if (t->base_type == GLSL_TYPE_ARRAY) {
      buffer = print_type(buffer, t->fields.array, true);
      if (arraySize)
         buffer = talloc_asprintf_append(buffer, "[%u]", t->length);
   } else if ((t->base_type == GLSL_TYPE_STRUCT)
	      && (strncmp("gl_", t->name, 3) != 0)) {
      buffer = talloc_asprintf_append(buffer, "%s", t->name);
   } else {
      buffer = talloc_asprintf_append(buffer, "%s", t->name);
   }
   return buffer;
}

static char*
print_type_post(char* buffer, const glsl_type *t, bool arraySize)
{
	if (t->base_type == GLSL_TYPE_ARRAY) {
		if (!arraySize)
			buffer = talloc_asprintf_append(buffer, "[%u]", t->length);
	}
	return buffer;
}


void ir_print_glsl_visitor::visit(ir_variable *ir)
{
   const char *const cent = (ir->centroid) ? "centroid " : "";
   const char *const inv = (ir->invariant) ? "invariant " : "";
   const char *const mode[3][6] = 
   {
	{ "", "uniform ", "in ",        "out ",     "inout ", "" },
	{ "", "uniform ", "attribute ", "varying ", "inout ", "" },
	{ "", "uniform ", "varying ",   "out ",     "inout ", "" },
   };
   const char *const interp[] = { "", "flat ", "noperspective " };

   buffer = talloc_asprintf_append(buffer, "%s%s%s%s",
	  cent, inv, mode[this->mode][ir->mode], interp[ir->interpolation]);
   buffer = print_type(buffer, ir->type, false);
   buffer = talloc_asprintf_append(buffer, " %s", ir->name);
   buffer = print_type_post(buffer, ir->type, false);
}


void ir_print_glsl_visitor::visit(ir_function_signature *ir)
{
   buffer = print_type(buffer, ir->return_type, true);
   buffer = talloc_asprintf_append(buffer, " %s (", ir->function_name());

   if (!ir->parameters.is_empty())
   {
	   buffer = talloc_asprintf_append(buffer, "\n");

	   indentation++;
	   bool first = true;
	   foreach_iter(exec_list_iterator, iter, ir->parameters) {
		  ir_variable *const inst = (ir_variable *) iter.get();

		  if (!first)
			  buffer = talloc_asprintf_append(buffer, ",\n");
		  indent();
		  inst->accept(this);
		  first = false;
	   }
	   indentation--;

	   buffer = talloc_asprintf_append(buffer, "\n");
	   indent();
   }

   if (ir->body.is_empty())
   {
	   buffer = talloc_asprintf_append(buffer, ");\n");
	   return;
   }

   buffer = talloc_asprintf_append(buffer, ")\n");

   indent();
   buffer = talloc_asprintf_append(buffer, "{\n");
   indentation++;

   foreach_iter(exec_list_iterator, iter, ir->body) {
      ir_instruction *const inst = (ir_instruction *) iter.get();

      indent();
      inst->accept(this);
	  buffer = talloc_asprintf_append(buffer, ";\n");
   }
   indentation--;
   indent();
   buffer = talloc_asprintf_append(buffer, "}\n");
}


void ir_print_glsl_visitor::visit(ir_function *ir)
{
   bool found_non_builtin_proto = false;

   foreach_iter(exec_list_iterator, iter, *ir) {
      ir_function_signature *const sig = (ir_function_signature *) iter.get();
      if (sig->is_defined || !sig->is_built_in)
	 found_non_builtin_proto = true;
   }
   if (!found_non_builtin_proto)
      return;

   PrintGlslMode oldMode = this->mode;
   this->mode = kPrintGlslNone;

   foreach_iter(exec_list_iterator, iter, *ir) {
      ir_function_signature *const sig = (ir_function_signature *) iter.get();

      indent();
      sig->accept(this);
      buffer = talloc_asprintf_append(buffer, "\n");
   }

   this->mode = oldMode;

   indent();
}


static const char *const operator_glsl_strs[] = {
	"~",
	"!",
	"-",
	"abs",
	"sign",
	"1.0/",
	"inversesqrt",
	"sqrt",
	"exp",
	"log",
	"exp2",
	"log2",
	"int",
	"float",
	"bool",
	"float",
	"bool",
	"int",
	"float",
	"trunc",
	"ceil",
	"floor",
	"fract",
	"sin",
	"cos",
	"dFdx",
	"dFdy",
	"+",
	"-",
	"*",
	"/",
	"%",
	"<",
	">",
	"<=",
	">=",
	"==",
	"!=",
	"<<",
	">>",
	"&",
	"^",
	"|",
	"&&",
	"^^",
	"||",
	"dot",
	"cross",
	"min",
	"max",
	"pow",
};

void ir_print_glsl_visitor::visit(ir_expression *ir)
{
	if (ir->get_num_operands() == 1) {
		if (ir->operation >= ir_unop_f2i && ir->operation <= ir_unop_u2f) {
			buffer = print_type(buffer, ir->type, true);
			buffer = talloc_asprintf_append(buffer, "(");
		} else {
			buffer = talloc_asprintf_append(buffer, "%s(", operator_glsl_strs[ir->operation]);
		}
		if (ir->operands[0])
			ir->operands[0]->accept(this);
		buffer = talloc_asprintf_append(buffer, ")");
	}
	else {
		buffer = talloc_asprintf_append(buffer, "(");
		if (ir->operands[0])
			ir->operands[0]->accept(this);

		buffer = talloc_asprintf_append(buffer, " %s ", ir->operator_string());

		if (ir->operands[1])
			ir->operands[1]->accept(this);
		buffer = talloc_asprintf_append(buffer, ")");
	}
}


void ir_print_glsl_visitor::visit(ir_texture *ir)
{
   buffer = talloc_asprintf_append(buffer, "(%s ", ir->opcode_string());

   ir->sampler->accept(this);
   buffer = talloc_asprintf_append(buffer, " ");

   ir->coordinate->accept(this);

   buffer = talloc_asprintf_append(buffer, " (%d %d %d) ", ir->offsets[0], ir->offsets[1], ir->offsets[2]);

   if (ir->op != ir_txf) {
      if (ir->projector)
	 ir->projector->accept(this);
      else
	 buffer = talloc_asprintf_append(buffer, "1");

      if (ir->shadow_comparitor) {
	 buffer = talloc_asprintf_append(buffer, " ");
	 ir->shadow_comparitor->accept(this);
      } else {
	 buffer = talloc_asprintf_append(buffer, " ()");
      }
   }

   buffer = talloc_asprintf_append(buffer, " ");
   switch (ir->op)
   {
   case ir_tex:
      break;
   case ir_txb:
      ir->lod_info.bias->accept(this);
      break;
   case ir_txl:
   case ir_txf:
      ir->lod_info.lod->accept(this);
      break;
   case ir_txd:
      buffer = talloc_asprintf_append(buffer, "(");
      ir->lod_info.grad.dPdx->accept(this);
      buffer = talloc_asprintf_append(buffer, " ");
      ir->lod_info.grad.dPdy->accept(this);
      buffer = talloc_asprintf_append(buffer, ")");
      break;
   };
   buffer = talloc_asprintf_append(buffer, ")");
}


void ir_print_glsl_visitor::visit(ir_swizzle *ir)
{
   const unsigned swiz[4] = {
      ir->mask.x,
      ir->mask.y,
      ir->mask.z,
      ir->mask.w,
   };

	if (ir->val->type == glsl_type::float_type)
	{
		if (ir->mask.num_components != 1)
		{
			buffer = print_type(buffer, ir->type, true);
			buffer = talloc_asprintf_append(buffer, "(");
		}
	}

	unsigned oldWriteMask = this->writeMask;
	this->writeMask = ~0;
	ir->val->accept(this);
	this->writeMask = oldWriteMask;
	
	if (ir->val->type == glsl_type::float_type)
	{
		if (ir->mask.num_components != 1)
		{
			buffer = talloc_asprintf_append(buffer, ")");
		}
		return;
	}

   buffer = talloc_asprintf_append(buffer, ".");
   for (unsigned i = 0; i < ir->mask.num_components; i++) {
	   if (this->writeMask & (1<<i))
		buffer = talloc_asprintf_append(buffer, "%c", "xyzw"[swiz[i]]);
   }
}


void ir_print_glsl_visitor::visit(ir_dereference_variable *ir)
{
   ir_variable *var = ir->variable_referenced();
   buffer = talloc_asprintf_append(buffer, "%s", var->name);
}


void ir_print_glsl_visitor::visit(ir_dereference_array *ir)
{
   ir->array->accept(this);
   buffer = talloc_asprintf_append(buffer, "[");
   ir->array_index->accept(this);
   buffer = talloc_asprintf_append(buffer, "]");
}


void ir_print_glsl_visitor::visit(ir_dereference_record *ir)
{
   ir->record->accept(this);
   buffer = talloc_asprintf_append(buffer, ".%s", ir->field);
}


void ir_print_glsl_visitor::visit(ir_assignment *ir)
{
   if (ir->condition)
   {
      ir->condition->accept(this);
	  buffer = talloc_asprintf_append(buffer, " ");
   }

   ir->lhs->accept(this);

   char mask[5];
   unsigned j = 0;
   const glsl_type* lhsType = ir->lhs->type;
   const glsl_type* rhsType = ir->rhs->type;
   unsigned oldWriteMask = this->writeMask;
   bool hasWriteMask = false;
   if (ir->lhs->type->vector_elements > 1 && ir->write_mask != (1<<ir->lhs->type->vector_elements)-1)
   {
	   for (unsigned i = 0; i < 4; i++) {
		   if ((ir->write_mask & (1 << i)) != 0) {
			   mask[j] = "xyzw"[i];
			   j++;
		   }
	   }
	   lhsType = glsl_type::get_instance(lhsType->base_type, j, 1);
	   this->writeMask = ~0;
	   ir_swizzle* sw = ir->rhs->as_swizzle();
	   if (sw && sw->val->type != glsl_type::float_type)
	   {
		   this->writeMask = ir->write_mask;
		   const unsigned swiz[4] = {
			   sw->mask.x,
			   sw->mask.y,
			   sw->mask.z,
			   sw->mask.w,
		   };
		   unsigned k = 0;
		   for (unsigned i = 0; i < sw->mask.num_components; i++) {
			   if ((this->writeMask & (1<<i)) /*&& (swiz[i] == i)*/)
				   ++k;
		   }
		   assert (k);
		   rhsType = glsl_type::get_instance(rhsType->base_type, k, 1);
	   }
   }
   mask[j] = '\0';
   if (mask[0])
   {
	   buffer = talloc_asprintf_append(buffer, ".%s", mask);
	   hasWriteMask = true;
   }

   buffer = talloc_asprintf_append(buffer, " = ");

   bool typeMismatch = (lhsType != rhsType);
   const bool addSwizzle = hasWriteMask && typeMismatch;
   if (typeMismatch)
   {
	   if (!addSwizzle)
		buffer = print_type(buffer, lhsType, true);
	   buffer = talloc_asprintf_append(buffer, "(");
   }

   ir->rhs->accept(this);

   if (typeMismatch)
   {
	   buffer = talloc_asprintf_append(buffer, ")");
	   if (addSwizzle)
		   buffer = talloc_asprintf_append(buffer, ".%s", mask);
   }

   this->writeMask = oldWriteMask;
}


void ir_print_glsl_visitor::visit(ir_constant *ir)
{
	if (ir->type == glsl_type::float_type)
	{
		buffer = talloc_asprintf_append(buffer, "%f", ir->value.f[0]);
		return;
	}
	else if (ir->type == glsl_type::int_type)
	{
		buffer = talloc_asprintf_append(buffer, "%d", ir->value.i[0]);
		return;
	}
	else if (ir->type == glsl_type::uint_type)
	{
		buffer = talloc_asprintf_append(buffer, "%u", ir->value.u[0]);
		return;
	}

   const glsl_type *const base_type = ir->type->get_base_type();

   buffer = print_type(buffer, ir->type, true);
   buffer = talloc_asprintf_append(buffer, "(");

   if (ir->type->is_array()) {
      for (unsigned i = 0; i < ir->type->length; i++)
	 ir->get_array_element(i)->accept(this);
   } else {
      for (unsigned i = 0; i < ir->type->components(); i++) {
	 if (i != 0)
	    buffer = talloc_asprintf_append(buffer, ", ");
	 switch (base_type->base_type) {
	 case GLSL_TYPE_UINT:  buffer = talloc_asprintf_append(buffer, "%u", ir->value.u[i]); break;
	 case GLSL_TYPE_INT:   buffer = talloc_asprintf_append(buffer, "%d", ir->value.i[i]); break;
	 case GLSL_TYPE_FLOAT: buffer = talloc_asprintf_append(buffer, "%f", ir->value.f[i]); break;
	 case GLSL_TYPE_BOOL:  buffer = talloc_asprintf_append(buffer, "%d", ir->value.b[i]); break;
	 default: assert(0);
	 }
      }
   }
   buffer = talloc_asprintf_append(buffer, ")");
}


void
ir_print_glsl_visitor::visit(ir_call *ir)
{
   buffer = talloc_asprintf_append(buffer, "%s (", ir->callee_name());
   bool first = true;
   foreach_iter(exec_list_iterator, iter, *ir) {
      ir_instruction *const inst = (ir_instruction *) iter.get();
	  if (!first)
		  buffer = talloc_asprintf_append(buffer, ", ");
      inst->accept(this);
	  first = false;
   }
   buffer = talloc_asprintf_append(buffer, ")");
}


void
ir_print_glsl_visitor::visit(ir_return *ir)
{
   buffer = talloc_asprintf_append(buffer, "return");

   ir_rvalue *const value = ir->get_value();
   if (value) {
      buffer = talloc_asprintf_append(buffer, " ");
      value->accept(this);
   }
}


void
ir_print_glsl_visitor::visit(ir_discard *ir)
{
   buffer = talloc_asprintf_append(buffer, "discard");

   if (ir->condition != NULL) {
      buffer = talloc_asprintf_append(buffer, " TODO ");
      ir->condition->accept(this);
   }
}


void
ir_print_glsl_visitor::visit(ir_if *ir)
{
   buffer = talloc_asprintf_append(buffer, "if (");
   ir->condition->accept(this);

   buffer = talloc_asprintf_append(buffer, ") {\n");
   indentation++;

   foreach_iter(exec_list_iterator, iter, ir->then_instructions) {
      ir_instruction *const inst = (ir_instruction *) iter.get();

      indent();
      inst->accept(this);
      buffer = talloc_asprintf_append(buffer, ";\n");
   }

   indentation--;
   indent();
   buffer = talloc_asprintf_append(buffer, "}");

   if (!ir->else_instructions.is_empty())
   {
	   buffer = talloc_asprintf_append(buffer, " else {\n");
	   indentation++;

	   foreach_iter(exec_list_iterator, iter, ir->else_instructions) {
		  ir_instruction *const inst = (ir_instruction *) iter.get();

		  indent();
		  inst->accept(this);
		  buffer = talloc_asprintf_append(buffer, ";\n");
	   }
	   indentation--;
	   indent();
	   buffer = talloc_asprintf_append(buffer, "}");
   }
}


void
ir_print_glsl_visitor::visit(ir_loop *ir)
{
	bool noData = (ir->counter == NULL && ir->from == NULL && ir->to == NULL && ir->increment == NULL);
	if (noData) {
		buffer = talloc_asprintf_append(buffer, "while (true) {\n");
		indentation++;
		foreach_iter(exec_list_iterator, iter, ir->body_instructions) {
			ir_instruction *const inst = (ir_instruction *) iter.get();
			indent();
			inst->accept(this);
			buffer = talloc_asprintf_append(buffer, ";\n");
		}
		indentation--;
		indent();
		buffer = talloc_asprintf_append(buffer, "}");
		return;
	}

   buffer = talloc_asprintf_append(buffer, "( TODO loop (");
   if (ir->counter != NULL)
      ir->counter->accept(this);
   buffer = talloc_asprintf_append(buffer, ") (");
   if (ir->from != NULL)
      ir->from->accept(this);
   buffer = talloc_asprintf_append(buffer, ") (");
   if (ir->to != NULL)
      ir->to->accept(this);
   buffer = talloc_asprintf_append(buffer, ") (");
   if (ir->increment != NULL)
      ir->increment->accept(this);
   buffer = talloc_asprintf_append(buffer, ") (\n");
   indentation++;

   foreach_iter(exec_list_iterator, iter, ir->body_instructions) {
      ir_instruction *const inst = (ir_instruction *) iter.get();

      indent();
      inst->accept(this);
      buffer = talloc_asprintf_append(buffer, ";\n");
   }
   indentation--;
   indent();
   buffer = talloc_asprintf_append(buffer, "))\n");
}


void
ir_print_glsl_visitor::visit(ir_loop_jump *ir)
{
   buffer = talloc_asprintf_append(buffer, "%s", ir->is_break() ? "break" : "continue");
}
