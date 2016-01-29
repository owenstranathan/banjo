// Copyright (c) 2015-2016 Andrew Sutton
// All rights reserved

#include "parser.hpp"
#include "scope.hpp"
#include "overload.hpp"
#include "print.hpp"

#include <iostream>


namespace banjo
{

// If we have an unassigned list of template parameters, then they
// provide the context for this declaration. Transform the declaration
// into a template. Clear the parameters so they aren't "re-used" for
// a nested declaration.
//
// If we are not parsing a template, no changes are made to d.
Decl&
Parser::templatize_declaration(Decl& d)
{
  if (state.template_parms) {
    // Build the template.
    Template_decl& tmp = build.make_template(*state.template_parms, d);
    state.template_parms = nullptr;

    // Apply constraints, if any.
    if (state.template_cons) {
      tmp.cons = state.template_cons;
      state.template_cons = nullptr;
    }
    return tmp;
  }
  return d;
}


// -------------------------------------------------------------------------- //
// Declaration
//
// TODO: Move this into a separate semantic module?

// Save `d` as a new declaration in the given overload set.
//
// TODO: Implement me.
//
// TODO: Actually check for overloading and re-definition
// errors.
Decl*
declare(Overload_set& ovl, Decl& d)
{
  lingo_unimplemented();
  return nullptr;
}


// Try to declare a name binding in the current scope.
//
// FIXME: The specific rules probably depend on a) the kind of scope,
// and b) the kind of declaration.
//
// FIXME: Check for re-declaration and overloading. This probably
// requires that we match on the entity declared.
//
// FIXME: If `d`'s name is a qualified-id, then we need to adjust
// the context to that specified by `d`s nested name specifier.
Decl*
Parser::declare(Scope& scope, Decl& d)
{
  // Declare the ajusted declaration.
  if (Overload_set* ovl = scope.lookup(d.declared_name())) {
    return banjo::declare(*ovl, d);
  } else {
    scope.bind(d);
    return nullptr;
  }
}


// -------------------------------------------------------------------------- //
// Declarators

// FIXME: Is there really anything interesting to do here?
// Maybe if the name is qualified, guarantee that it was
// previously defined.
Name&
Parser::on_declarator(Name& n)
{
  return n;
}


// -------------------------------------------------------------------------- //
// Variables


Decl&
Parser::on_variable_declaration(Token, Name& n, Type& t)
{
  Decl& d1 = build.make_variable(n, t);
  Decl& d2 = templatize_declaration(d1);
  declare(current_scope(), d2);
  return d2;
}


// -------------------------------------------------------------------------- //
// Functions


// FIXME: Move these into a new module, sema_def.cpp, Or put
// them in sema_init?


// A helper function that hides the ugliness of assinging the
// function definition.
static inline Def&
define_function(Decl& decl, Def& def)
{
  Decl* d = &decl.parameterized_declaration();
  if (Function_decl* f = as<Function_decl>(d))
    return *(f->def = &def);
  lingo_unreachable();
}


// Define a function, class, enum, or entity.
static inline Def&
define_entity(Decl& decl, Def& def)
{
  Decl* d = &decl.parameterized_declaration();
  if (Function_decl* f = as<Function_decl>(d))
    return *(f->def = &def);
  if (Class_decl* c = as<Class_decl>(d))
    return *(c->def = &def);
  lingo_unreachable();
}




Decl&
Parser::on_function_declaration(Token, Name& n, Decl_list& ps, Type& t)
{
  Decl& d1 = build.make_function(n, ps, t);
  Decl& d2 = templatize_declaration(d1);
  declare(current_scope(), d2);
  return d2;
}


Object_parm&
Parser::on_function_parameter(Name& n, Type& t)
{
  Object_parm& parm = build.make_object_parm(n, t);
  declare(current_scope(), parm);
  return parm;
}


Def&
Parser::on_function_definition(Decl& d, Stmt& s)
{
  Def& def = build.make_function_definition(s);
  return define_function(d, def);
}


Def&
Parser::on_deleted_definition(Decl& d)
{
  Def& def = build.make_deleted_definition();
  return define_entity(d, def);
}


Def&
Parser::on_defaulted_definition(Decl& d)
{
  Def& def = build.make_defaulted_definition();
  return define_function(d, def);
}


// -------------------------------------------------------------------------- //
// Classes

// FIXME: Differentiate between classes and structures.
Decl&
Parser::on_class_declaration(Token tok, Name& n)
{
  Decl& d1 = build.make_class(n);
  Decl& d2 = templatize_declaration(d1);
  declare(current_scope(), d2);
  return d2;
}


Def&
Parser::on_class_definition(Decl& d, Decl_list& ds)
{
  Def& def = build.make_class_definition(ds);
  return define_entity(d, def);
}


// -------------------------------------------------------------------------- //
// Namespaces


Decl&
Parser::on_namespace_declaration(Token, Name&, Decl_list&)
{
  lingo_unimplemented();
}


// -------------------------------------------------------------------------- //
// Templates

Type_parm&
Parser::on_type_template_parameter(Name& n)
{
  Type_parm& parm = build.make_type_parameter(n);
  declare(current_scope(), parm);
  return parm;
}


Type_parm&
Parser::on_type_template_parameter(Name& n, Type& t)
{
  Type_parm& parm = build.make_type_parameter(n, t);
  declare(current_scope(), parm);
  return parm;
}


// -------------------------------------------------------------------------- //
// Translation units


// Merge the parsed declarations into the global namespace.
Namespace_decl&
Parser::on_translation_unit(Decl_list& ds)
{
  Namespace_decl& ns = cxt.global_namespace();
  ns.decls.append(ds.begin(), ds.end());
  return ns;
}


} // namespace banjo
