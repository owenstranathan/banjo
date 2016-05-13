// Copyright (c) 2015-2016 Andrew Sutton
// All rights reserved

#include "parser.hpp"
#include "printer.hpp"
#include "ast.hpp"
#include "type.hpp"

#include <iostream>


namespace banjo
{

// Elaborate the type of each declaration in turn. Note that elaboration
// and "skip forward" if the type of one declaration depends on the type
// or definition of another defined after it.
void
Parser::elaborate_declarations(Stmt_list& ss)
{
  for (Stmt& s : ss) {
    elaborate_declaration(s);
  }
}

// If the statement is a declaration, elaborate its declared type.
void
Parser::elaborate_declaration(Stmt& s)
{
  if (Declaration_stmt* s1 = as<Declaration_stmt>(&s))
    elaborate_declaration(s1->declaration());
}


void
Parser::elaborate_declaration(Decl& d)
{
  struct fn
  {
    Parser& p;
    void operator()(Decl& d)          { lingo_unhandled(d); }
    void operator()(Object_decl& d)   { p.elaborate_object_declaration(d); }
    void operator()(Function_decl& d) { p.elaborate_function_declaration(d); }
    void operator()(Super_decl& d)    { p.elaborate_super_declaration(d); }
    void operator()(Class_decl& d)    { p.elaborate_class_declaration(d); }
    void operator()(Coroutine_decl& d){ p.elaborate_coroutine_declaration(d); }
  };
  apply(d, fn{*this});
}



void
Parser::elaborate_object_declaration(Object_decl& d)
{
  d.type_ = &elaborate_type(d.type());
}


void
Parser::elaborate_function_declaration(Function_decl& d)
{
  // Reset the list of implicit parameters.
  state.implicit_parms = {};

  // Elaborate the type of each parameter in turn. Note that this does
  // not declare the parameters, it just checks their types.
  Decl_list& parms = d.parameters();

  for (Decl& parm_decl : parms)
    elaborate_parameter_declaration(cast<Object_parm>(parm_decl));

  // Collect Open-Methods
  for (Decl& parm_decl : parms)
  {
    if (parm_decl.specifiers() == virtual_spec) {
      Simple_id & id = cast<Simple_id>(d.name());
      if(cxt.bom.find(id.symbol().spelling()) == cxt.bom.end())
      {
        cxt.bom[id.symbol().spelling()] = &d;
      }
      cxt.omd[id.symbol().spelling()].push_back(&d);
      break;
    }
  }
  for(auto && x : cxt.bom)
  {
    std::cout << "Base Methods:" << std::endl;
    std::cout << x.first << " : ";
    std::cout << "(";
    for(auto & a : x.second->parms_)
    {
      Simple_id & id = cast<Simple_id>(a.name());
      std::cout << id.symbol().spelling() << " : " << a.type() << ", ";
    }
    std::cout << ")" << '\n';
  }
  for(auto && x : cxt.omd)
  {
    std::cout << "All Methods:" << std::endl;
    for(auto & a : x.second)
    {
      std::cout << x.first << " : ";
      std::cout << "(";
      for(auto& b : a->parms_)
      {
        Simple_id & id = cast<Simple_id>(b.name());
        std::cout << id.symbol().spelling() << " : " << b.type() << ", ";
      }
      std::cout << ")" << '\n';
    }
  }

  // Elaborate the return type.
  //
  // FIXME: If the return type shares a placehoder name with a parameter,
  // then that's not a placeholder. We need to rewrite the type.
  Type& ret = elaborate_type(d.return_type());

  // Rebuild the function type and update the declaration.
  d.type_ = &cxt.get_function_type(parms, ret);

  // If necessary, make the function a template.
  if (state.implicit_parms.size()) {
    Decl& tmp = cxt.make_template(state.implicit_parms, d);

    // FIXME: Actually make this a declaration! We probably need to
    // replace this entity in the declaration list with its new
    // template. We also need to update the overload set with the same.
    (void)tmp;
  }

}


Type& rewrite_parameter_type(Context&, Type&, Decl_list&);


// Transform the auto type into a template type parameter.
Type&
rewrite_parameter_type(Context& cxt, Auto_type& t, Decl_list& ds)
{
  Type_parm& d = cxt.make_type_parameter(t.name());
  ds.push_back(d);
  return cxt.get_typename_type(d);
}


Type&
rewrite_parameter_type(Context& cxt, Reference_type& t, Decl_list& ds)
{
  Type& t1 = rewrite_parameter_type(cxt, t.type(), ds);
  return cxt.get_reference_type(t1);
}


Type&
rewrite_parameter_type(Context& cxt, Qualified_type& t, Decl_list& ds)
{
  Type& t1 = rewrite_parameter_type(cxt, t.type(), ds);
  return cxt.get_qualified_type(t1, t.qualifiers());
}


// FIXME: This probably needs an entire (implicit) template declaration
// to maintain both the list and the scpoe. This will also give us the
// ability to map names to types for constrained placeholders.
//
// TODO: Finish writing this.
Type&
rewrite_parameter_type(Context& cxt, Type& t, Decl_list& ds)
{
  struct fn
  {
    Context&   cxt;
    Decl_list& ds;
    Type& operator()(Type& t)           { return t; }
    Type& operator()(Auto_type& t)      { return rewrite_parameter_type(cxt, t, ds); }
    Type& operator()(Reference_type& t) { return rewrite_parameter_type(cxt, t, ds); }
    Type& operator()(Qualified_type& t) { return rewrite_parameter_type(cxt, t, ds); }
  };
  return apply(t, fn{cxt, ds});
}


void
Parser::elaborate_parameter_declaration(Object_parm& p)
{
  // Update the type of the declaration.
  //
  // FIXME: Do I really need to rewrite the type. I'm not actually
  // changing anything (yet), just saving implicit parameters.
  Type& t1 = elaborate_type(p.type());
  Type& t2 = rewrite_parameter_type(cxt, t1, state.implicit_parms);
  p.type_ = &t2;

  // Create template parameters for all of the placeholders in
  // the type of the parameter.
  //
  // FIXME: This isn't sustainable.
  // Type_list types = get_placeholders(p.type());
  // for (Type const& )
}


// Elaborate the kind of a type.
void
Parser::elaborate_class_declaration(Class_decl& d)
{
  d.kind_ = &elaborate_type(d.kind());
  //std::cout << d.kind();
}

void
Parser::elaborate_super_declaration(Super_decl& d)
{
  d.type_ = &elaborate_type(d.type());
  if(!is<Class_type>(d.type_))
  {
    lingo_alert(false, "A class cannot inherit from non-class types!");
  }
}


Type&
Parser::elaborate_type(Type& t)
{
  if (Unparsed_type* soup = as<Unparsed_type>(&t)) {
    Save_input_location loc(cxt);
    Token_stream ts(soup->tokens());
    Parser parse(cxt, ts);
    return parse.type();
  }
  return t;
}

void
Parser::elaborate_coroutine_declaration(Coroutine_decl &d)
{
  // Elaborate the parameters
  Decl_list& parms = d.parameters();
  for (Decl& d : parms)
    elaborate_parameter_declaration(cast<Object_parm>(d));
  // Elaborate the return type of the coroutine
  d.ret_ = &elaborate_type(d.type());
}

} // namespace banjo
