#include "glsl_optimizer.h"
#include "ast.h"
#include "glsl_parser_extras.h"
#include "glsl_parser.h"
#include "ir_optimization.h"
#include "ir_print_glsl_visitor.h"
#include "ir_print_visitor.h"
#include "program.h"

extern "C" struct gl_shader *
_mesa_new_shader(GLcontext *ctx, GLuint name, GLenum type);

// Copied from shader_api.c for the stand-alone compiler.
struct gl_shader *
_mesa_new_shader(GLcontext *ctx, GLuint name, GLenum type)
{
   struct gl_shader *shader;
   assert(type == GL_FRAGMENT_SHADER || type == GL_VERTEX_SHADER);
   shader = talloc_zero(NULL, struct gl_shader);
   if (shader) {
      shader->Type = type;
      shader->Name = name;
      shader->RefCount = 1;
   }
   return shader;
}


struct glslopt_ctx {
	glslopt_ctx () {
		mem_ctx = talloc_new (NULL);
	}
	~glslopt_ctx() {
		talloc_free (mem_ctx);
	}
	void* mem_ctx;
};

glslopt_ctx* glslopt_initialize ()
{
	return new glslopt_ctx();
}

void glslopt_cleanup (glslopt_ctx* ctx)
{
	delete ctx;
	_mesa_glsl_release_types();
	_mesa_glsl_release_functions();
}



struct glslopt_shader {
	static void* operator new(size_t size, void *ctx)
	{
		void *node;
		node = talloc_size(ctx, size);
		assert(node != NULL);
		return node;
	}
	static void operator delete(void *node)
	{
		talloc_free(node);
	}

	glslopt_shader ()
		: rawOutput(0)
		, optimizedOutput(0)
		, status(false)
	{
		infoLog = "Shader not compiled yet";
	}

	char*	rawOutput;
	char*	optimizedOutput;
	char*	infoLog;
	bool	status;
};

static inline void debug_print_ir (const char* name, exec_list* ir, _mesa_glsl_parse_state* state)
{
	#if 0
	printf("**** %s:\n", name);
	_mesa_print_ir (ir, state);
	validate_ir_tree(ir);
	#endif
}

glslopt_shader* glslopt_optimize (glslopt_ctx* ctx, glslopt_shader_type type, const char* shaderSource)
{
	glslopt_shader* shader = new (ctx->mem_ctx) glslopt_shader ();

	GLenum glType = 0;
	PrintGlslMode printMode;
	switch (type) {
	case kGlslOptShaderVertex: glType = GL_VERTEX_SHADER; printMode = kPrintGlslVertex; break;
	case kGlslOptShaderFragment: glType = GL_FRAGMENT_SHADER; printMode = kPrintGlslFragment; break;
	}
	if (!glType)
	{
		shader->infoLog = talloc_asprintf (ctx->mem_ctx, "Unknown shader type %d", (int)type);
		shader->status = false;
		return shader;
	}

	_mesa_glsl_parse_state* state = new (ctx->mem_ctx) _mesa_glsl_parse_state (NULL, glType, ctx->mem_ctx);
	state->error = 0;

	_mesa_glsl_lexer_ctor (state, shaderSource);
	_mesa_glsl_parse (state);
	_mesa_glsl_lexer_dtor (state);

	exec_list* ir = new (ctx->mem_ctx) exec_list();

	if (!state->error && !state->translation_unit.is_empty())
		_mesa_ast_to_hir (ir, state);

	// Un-optimized output
	if (!state->error) {
		validate_ir_tree(ir);
		shader->rawOutput = _mesa_print_ir_glsl(ir, state, talloc_strdup(ctx->mem_ctx, ""), printMode);
	}

	// Optimization passes
	if (!state->error && !ir->is_empty())
	{
		bool progress;
		do {
			progress = false;
			debug_print_ir ("Initial", ir, state);
			progress = do_function_inlining(ir) || progress; debug_print_ir ("After inlining", ir, state);
			progress = do_dead_functions(ir) || progress; debug_print_ir ("After dead functions", ir, state);
			progress = do_structure_splitting(ir) || progress; debug_print_ir ("After struct splitting", ir, state);
			progress = do_if_simplification(ir) || progress; debug_print_ir ("After if simpl", ir, state);
			progress = do_copy_propagation(ir) || progress; debug_print_ir ("After copy propagation", ir, state);
			progress = do_dead_code_local(ir) || progress; debug_print_ir ("After dead code local", ir, state);
			progress = do_dead_code_unlinked(ir) || progress; debug_print_ir ("After dead code unlinked", ir, state);
			progress = do_tree_grafting(ir) || progress; debug_print_ir ("After tree grafting", ir, state);
			progress = do_constant_propagation(ir) || progress; debug_print_ir ("After const propagation", ir, state);
			progress = do_constant_variable_unlinked(ir) || progress; debug_print_ir ("After const variable unlinked", ir, state);
			progress = do_constant_folding(ir) || progress; debug_print_ir ("After const folding", ir, state);
			progress = do_algebraic(ir) || progress; debug_print_ir ("After algebraic", ir, state);
			progress = do_vec_index_to_swizzle(ir) || progress; debug_print_ir ("After vec index to swizzle", ir, state);
			//progress = do_vec_index_to_cond_assign(ir) || progress; debug_print_ir ("After vec index to cond assign", ir, state);
			progress = do_swizzle_swizzle(ir) || progress; debug_print_ir ("After swizzle swizzle", ir, state);
			progress = do_noop_swizzle(ir) || progress; debug_print_ir ("After noop swizzle", ir, state);
		} while (progress);

		validate_ir_tree(ir);
	}

	// Final optimized output
	if (!state->error)
	{
		shader->optimizedOutput = _mesa_print_ir_glsl(ir, state, talloc_strdup(ctx->mem_ctx, ""), printMode);
	}

	shader->status = !state->error;
	shader->infoLog = state->info_log;

	talloc_free (ir);
	talloc_free (state);

	return shader;
}

void glslopt_shader_delete (glslopt_shader* shader)
{
	delete shader;
}

bool glslopt_get_status (glslopt_shader* shader)
{
	return shader->status;
}

const char* glslopt_get_output (glslopt_shader* shader)
{
	return shader->optimizedOutput;
}

const char* glslopt_get_raw_output (glslopt_shader* shader)
{
	return shader->rawOutput;
}

const char* glslopt_get_log (glslopt_shader* shader)
{
	return shader->infoLog;
}
