#include <cassert>
#include <ctype.h>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <cmath>
#include <string>
#include <string.h>
#include <variant>
#include <vector>

#include "cell.h"

#include "io-abstract.h"
#include "io-utils.h"
//#include "neotypes.h"
//#include "ref.h"
#include "sheet.h"
//#include "xcept.h"

#include "parser-2019.h"

using namespace std;


///////////////////////////////////////////////////////////////////////////////////////////////////
// TYPE DECLARATIONS



extern map<string, parse_function_t> funcmap;

void unknown_function(string function_name)
{
	cerr << "Unknown function " << function_name << "\n";
	throw 667;
}

funptr fn_lookup(string function_name)
{
	if(funcmap.count(function_name) == 0)
		unknown_function(function_name);
	return &funcmap[function_name];
}

Expr::Expr()
{
	expr = empty_t{};
}

Expr::Expr(string fname, Expr x)
{
	FunCall fc;
	fc.fn = fn_lookup(fname);
	fc.args.push_back(x);
	this->expr = fc;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS

value_t eval(Expr expr);
num_t num_eval(Expr expr);
string str_eval(Expr expr);


void parse_error()
{
	throw ValErr(PARSE_ERR);
	//throw 666;
}

value_t do_plus(args_t args)
{
	num_t val = 0;
	for(auto& arg: args) val += num_eval(arg);
	return val;
}
value_t do_minus(args_t args)
{
	if(args.size() == 0) return 0;
	num_t val = num_eval(args[0]);
	if(args.size() == 1) return -val; // if there is only one argument, then return the negative of it
	for(int i = 1; i<args.size(); ++i) val -= num_eval(args[i]);
	return val;
}
value_t do_mul(args_t args)
{
	num_t val = 1.0;
	for(auto& arg: args) {
		val *=  num_eval(arg);
		//cout << "do_mul a and val " << a << " " << val << "\n";
	}
	return val;
}
value_t do_div(args_t args)
{
	if(args.size() == 0) return 0;
	num_t val = num_eval(args[0]);
	//cout << "do_div 1/val " << 1.0/val << "\n";
	if(args.size() == 1) return 1.0/val;
	for(int i = 1; i<args.size(); ++i) val /= num_eval(args[i]);
	return val;
}

value_t do_sqrt(args_t args)
{
	if(args.size() !=1) parse_error();
	num_t val = num_eval(args[0]);
	return sqrt(val);
}
value_t do_hypot(args_t args)
{
	if(args.size() !=2) parse_error();
	num_t v1 = num_eval(args[0]);
	num_t v2 = num_eval(args[1]);
	return sqrt(v1*v1 + v2*v2);
}
value_t do_plusfn(args_t args)
{
	num_t val = 0;
	for(auto& v: args) val += num_eval(v);
	return val;
}

value_t do_strlen(args_t args)
{
	if(args.size() !=1) parse_error();
	return strlen(str_eval(args.at(0)).c_str());
	//num_t val = num_eval(args[0]);
	//return sqrt(val);
}

map<string, parse_function_t> funcmap= {

	{"strlen", do_strlen},
	{"+", &do_plus},
	{"-", &do_minus},
	{"*", &do_mul},
	{"/", &do_div},
	{"sqrt", do_sqrt},
	{"hypot", do_hypot},
	{"plus", do_plusfn}
};


///////////////////////////////////////////////////////////////////////////////////////////////////
// LEXER

//enum Tokens { EOI, UNK, NUMBER, ID, PLUS };
enum Tokens { EOI = 128, NUMBER, ID, STR };


	static tokens_t 
tokenise (string str)
{
	tokens_t tokens;

	auto found = [&tokens](auto toketype, auto token) { tokens.push_back(make_pair(toketype, token)); };
	//cout << "Parsing: " << str << "\n";
	const char* cstr = str.c_str();
	int pos = 0;
	auto it = str.begin();
loop:
	string token;
	char ch = cstr[pos];
	if(ch == 0) {
		goto finis;
	} else if(isspace(ch)) {
		while(isspace(ch)) { ch = cstr[++pos]; }
	} else if ( isdigit(ch)) {
		while(isdigit(ch) || ch == '.' ) { token += ch; ch = cstr[++pos]; }
		found(NUMBER, token);
	} else if (isalpha(ch)) {
		while(isalnum(ch)) { token += ch; ch = cstr[++pos]; }
		found(ID, token);
		//cout << "found id: " << token << "\n";
	} else if(ch == '"') {
		while(1) {
			ch = cstr[++pos];
			if(ch == 0 || ch == '"') {pos++; break; }
			token += ch;
		}
		//cout << "tokenise string is <" << token << ">\n";
		found(STR, token);
	}else {
		token = ch;
		pos++;
		found(ch, token);
	}
	goto loop;
finis:
	found(EOI, "End of stream"); // Add this so that we can look ahead into the void
	return tokens;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// SCANNER (the "yacc" side of things)


void consume(char ch, tokens_t& tokes)
{
	if(ch == tokes.front().first)
		tokes.pop_front();
	else
		parse_error();
}

	template<class Q>
Q rest(Q qs)
{
	qs.pop_front();
	return qs;
}


typedef deque<string> ops_t;

// ARITHMETIC
// Adopt the algorithm at 
// https://www.engr.mun.ca/~theo/Misc/exp_parsing.htm#classic
// for computing arithmetic
//
// Here's the original derivations
//  E --> T {( "+" | "-" ) T}
//  T --> F {( "*" | "/" ) F}
//  F --> P ["^" F]
//  P --> v | "(" E ")" | "-" T
//
// Here's mine:
// I extend BNF with the notion of a function, prefixed by &
// {} zero or more repetitions
// [] optional
//  &M(o, X) --> X {o X} // e.g. &M(("+"|"-"), T) --> T { ("+"|"-") T }
//  E --> &M(( "<" | "<=" | ">" | ">=" | "==" | "!=" ), R)
//  R --> &M(( "+" | "-" ), T)
//  T --> &M(( "*" | "/" ), F)
//  F --> ["+"|"-"] (v | "(" E ")")



Expr parse_e (tokens_t& tokes);
Expr parse_t(tokens_t& tokes);

// parse a function
Expr parse_fn(string fname, tokens_t& tokes)
{
	//cout << "parse_fn name " << fname << "\n";
	auto fn = fn_lookup(fname);
	//cout << (*fn)(args_t{12}) << "\n";

	consume('(', tokes);
	FunCall fc;
	fc.fn = fn;
loop:
	if(tokes.front().first == ')') goto finis;
	fc.args.push_back(parse_e(tokes));
	if(tokes.front().first == ',') {
		consume(',', tokes);
		goto loop;
	} else if(tokes.front().first != ')')
		parse_error();

finis:
	consume(')', tokes);

	//args_t args{parse_e(tokes)};

	//fc.args = args;
	Expr x;
	x.expr = fc;

	//tokes.pop_front(); // rrb
	return x;
}

Expr parse_p(tokens_t& tokes)
{
	//Expr t{parse_t(tokes)};
	token_t toke = tokes.front();
	tokes.pop_front();
	switch(toke.first) {
		//case EOI:
		//	return Expr();
		case NUMBER:
			return Expr(stoi(toke.second));
		case STR:
			return Expr(toke.second);
		case ID: {
				 if(tokes.front().first == '(')
					 return parse_fn(toke.second, tokes);
				 else
					 parse_error(); // although could be a variable name
			 }
		case '(': {
				  //tokes.pop_front();
				  Expr x1{parse_e(tokes)};
				  if(tokes.front().first == ')')
					  tokes.pop_front();
				  else
					  parse_error();
				  return x1;
			  }
		case '-':
			  return Expr("-", parse_t(tokes));
		default:
			  parse_error();
	}
	return Expr(); // should never reach here
}
Expr parse_f(tokens_t& tokes) { return parse_p(tokes); }


Expr simplify(const FunCall& fc)
{
	if(fc.args.size() ==0)
		return Expr();

	if(fc.args.size() == 1)
		return Expr(fc.args[0]);

	Expr x;
	x.expr = fc;
	return x;
}
Expr _parse_t (tokens_t& tokes)
{
	FunCall fc;
	fc.fn = &funcmap["*"];

	fc.args.push_back(parse_f(tokes));
	//return fc;
	while(1) {
		auto nid = tokes.front().first;
		//if(nid == EOI) return fc;
		if(nid == '*') {
			tokes.pop_front();
			fc.args.push_back(parse_f(tokes));
		} else  if(nid == '/') {
			tokes.pop_front();
			Expr eneg =parse_f(tokes);
			FunCall fneg;
			fneg.fn = &funcmap["/"];
			fneg.args = args_t{eneg};
			Expr x2;
			x2.expr = fneg;
			fc.args.push_back(x2);		
		} else break; /* {
			return fc;
		}*/
	}
	return simplify(fc);
}

	Expr 
parse_t (tokens_t& tokes)
{
	return _parse_t(tokes);

	/*
	FunCall fc{_parse_t(tokes)};

	if(fc.args.size() == 0)
		return Expr();

	if(fc.args.size() == 1)
		return Expr(fc.args[0]);

	Expr x;
	x.expr = fc;
	return x;
	*/
}


Expr _parse_e(tokens_t& tokes)
{
	FunCall fc;
	fc.fn = &funcmap["+"];

	fc.args.push_back(parse_t(tokes));
	while(1) {
		auto nid = tokes.front().first;
		//cout << "nid is " << nid << "\n";
		//if(nid == EOI) break;
		//tokes.pop_front();
		if(nid == '+') {
			tokes.pop_front();
			//cout <<" nod is +\n";
			fc.args.push_back(parse_t(tokes));
		} else  if(nid == '-') {
			tokes.pop_front();
			Expr eneg =parse_t(tokes);
			FunCall fneg;
			fneg.fn = &funcmap["-"];
			fneg.args = args_t{eneg};
			Expr x2;
			x2.expr = fneg;
			fc.args.push_back(x2);		
		} else break; /* else {
			return fc;
			//parse_error();
		} */
	}
	return simplify(fc);
}
Expr parse_e (tokens_t& tokes)
{
	return _parse_e(tokes);
	/*
	FunCall fc{_parse_e(tokes)};

	if(fc.args.size() == 0)
		return Expr();

	if(fc.args.size() == 1)
		return Expr(fc.args[0]);

	Expr x;
	x.expr = fc;
	return x;
	*/

}



///////////////////////////////////////////////////////////////////////////////////////////////////
// EVAL

//template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
//template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

value_t eval (Expr expr)
{
	value_t val = 667;

	try {
		if(std::holds_alternative<value_t>(expr.expr))
			val = std::get<value_t>(expr.expr);
		else { // must be a function call		
			//auto &fn = std::get<FunCall>(expr.fn);
			auto &fc = std::get<FunCall>(expr.expr);
			auto fn = fc.fn;
			return (*fn)(fc.args);
		}
	} catch(ValErr ve) {
		val = err_t{ve.num()};
	}

	return val;
}

	template <class T>
T tox(value_t val, int errtype)
{
	if(std::holds_alternative<T>(val))
		return std::get<T>(val);
	else
		throw ValErr(errtype);
}

ValType get_value_t_type(value_t& val)
{
	if(is_nul(val)) 	return TYP_NUL;
	if(is_string(val)) 	return TYP_STR;
	if(is_num(val))		return TYP_FLT;
	if(is_err(val))		return TYP_ERR;
	ASSERT_UNCALLED();
	return TYP_NUL;
}

bool is_nul(value_t val) { return std::holds_alternative<empty_t>(val); }
bool is_err(value_t val) { return std::holds_alternative<err_t>(val); }
bool is_num(value_t val) { return std::holds_alternative<num_t>(val); }
bool is_string(value_t val) { return std::holds_alternative<string>(val); }
//num_t to_num (value_t v) { return std::get<num_t>(v); }
num_t to_num (value_t v) { return tox<num_t>(v, NON_NUMBER); }
err_t to_err(value_t v) { return tox<err_t>(v, ERR_CMD); }
num_t num_eval (Expr expr) { return to_num(eval(expr)); }
//string to_str1 (value_t v) { return std::get<string>(v); }
string to_str (value_t v) { return tox<string>(v, NON_STRING); }

string str_eval (Expr expr) { return to_str(eval(expr)); }
///////////////////////////////////////////////////////////////////////////////////////////////////

Expr parse_string(const std::string& s)
	//Expr parse_string(const string& s)
{
	tokens_t tokes{tokenise(s)};

	if constexpr (0) {
		for(auto& t:tokes) {
			cout << "Found: " << t.first << " " << t.second << "\n";

		}
	}

	if(tokes.size() == 0) return Expr(); // the empty expression

	try {
		return parse_e(tokes);
	} catch (ValErr ve) {
		err_t err{ve.num()};
		return Expr{err};
	}
}

int interpret(string s, string expected)
{
	cout << "Interpreting `" << s << "'\n";

	Expr expr{parse_string(s)};
	//cout << "point b\n";

	value_t v = eval(expr);
	string res = stringify_value_file_style(v);

	cout << "Evaluates to `" << res << "' ";
	if(res == expected) 
		cout << "PASS";
	else
		cout << "(s/b `" << expected << "') FAIL";
	cout << "\n\n";

	return 0;
}

std::string set_and_eval(CELLREF r, CELLREF c, const std::string& formula, bool display_it = false)
{
	CELL* cp = find_or_make_cell(r, c);
	cp->set_formula_text(formula);
	cp->eval_cell();
	//value_t val = eval(cp->parse_tree);
	//cp->sValue(val);
	if(display_it) // this is really cack-handed
		io_pr_cell(r, c, cp);

	return print_cell(cp);
}

int interpret(int r, int c, string s, string expecting)
{
	cout << "Interpreting R" << r << "C" << c << ": `" << s << "'\n";
	string res = set_and_eval(r,c, s);
	cout << "Result is `" << res << "' " ;
	cout << (res == expecting ? "PASS"s : "FAIL") << "\n\n";
	return 1;
}

int run_parser_2019_tests ()
{
	cout << "Running parser 2019 tests\n";

	use_parser_2019 = true;

	interpret("sqrt(4+5)+2", "5");
	//return 0;
	interpret("42", "42");
	interpret("42+3", "45");
	interpret("1+3+5+7", "16");
	interpret("1-3-5-7", "-14");
	interpret("1-3+5-7", "-4");
	interpret("1+2*3", "7");
	interpret("1+12/3", "5");
	interpret("1+12/3*2/2", "5");
	interpret("(1+2)*3", "9");
	interpret("-(1+2)*-3", "9");
	interpret("hypot(3,4)+1", "6");
	interpret("plus()+1", "1");
	interpret("plus(2)+1", "3");
	interpret("plus(2,3  +4  )  + 1", "10");
	interpret(" strlen(\"hello world\") ", "11");
	interpret("1+", "#PARSE_ERROR");
	interpret(" strlen(1) ", "#NON_STRING");

	interpret(1,1, "1+2", "3");
	interpret(1,1, "1+", "#PARSE_ERROR");
	interpret(1,1, "strlen(1)", "#NON_STRING");
	interpret(1,1, "1 2", "#PARSE_ERROR");
	interpret(1,1, "", "#NON_STRING");

	//value v = val;

	return 0;
}
