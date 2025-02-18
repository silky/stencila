#pragma once

#include <Rcpp.h>

#ifdef STENCILA_R_EMBED
    #include <RInside.h>
#endif

// Undefine some macros that R defines which clash
// with those used below
#undef Realloc
#undef Free
#undef ERROR

#include <stencila/context.hpp>
#include <stencila/string.hpp>

namespace Stencila {

class RException : public Exception {
public:
    RException(std::string message): Exception(message) {}
};

/*!
A `Context` for R.

Implements the methods of the Context class for the rendering of stencils in an
R environment. All the real functionality is done in an "R-side" Context class (see the R code)
and this class justs acts as a bridge between C++ and that code.

Uses the Rcpp::Language class which provides a much easier interface than using the 
'eval' R API function e.g. eval(name,R_GlobalEnv).
Note that although a function object has a () operator which calls a function in R I found difficulties
due to the way that it passes arguments (as a pairlist?). Using Rcpp::Language is more verbose but works.

There appear to  be several way to use Rcpp to get and call the R-side context method.
These include using the [] operator on the context and the () operator on a function object.
However, these don't always produce the expected results and so the best approach seems to be
to use the get() method, construct a Rcpp::Language object and then eval(). e.g.

    Rcpp::Language call(context_.get("method_name"),arg1,arg2);
    call.eval();

Note that when the method is being called with no arguments it appear to be necessary to consturct a Rcpp::Function 
object first:

    Rcpp::Language call(Rcpp::Function(context_.get("enter")));
    call.eval();

*/
class RContext : public Context {
public:

    std::string serve(void){
        return Component::serve(RContextType);
    }

    RContext& view(void){
        Component::view(RContextType);
        return *this;
    }

    static void class_init(void){
        Class::set(RContextType, {
            "RContext"
        });
    }

private:

    /**
     * Get R code used to implement a `RContext` on the R-side
     */
    static const char* code_(void){ 
        //return
        //    #include "r-context.R"
        //;
        return "";
    }

    #ifdef STENCILA_R_EMBED
        static RInside r_;
        static unsigned int contexts_;
    #endif

    std::string id_;

    /*!
    An Rcpp object which represents this context on the R "side"
    */
    Rcpp::Environment context_;
   
    static std::string arguments(void){
        return "";
    }

    static std::string arguments(std::string arg){
        replace_all(arg,"\"","\\\"");
        return '"'+arg+'"';
    }

    template<
        typename Arg
    >
    static std::string arguments(Arg arg){
        return string(arg);
    }

    template<
        typename Arg,
        typename... Args
    >
    static std::string arguments(Arg arg, Args... args){
        return arguments(arg) + "," + arguments(args...);
    }

    template<
        typename... Args
    >
    SEXP call_(const char* name,Args... args){
        #ifdef STENCILA_R_EMBED
            // Generate a call expression
            std::string call = id_+"$"+name+"("+arguments(args...)+")";
            try {
                return r_.parseEval(call);
            }
            catch(const std::runtime_error& exc) {
                // Rinside::parseEval throws a std::runtime_error with a message similar to "Error evaluating: context4233$execute(..." 
                // i.e. its message is for the call string above and gives few details. 
                // So, grab some more details and turn them into an RException
                std::string message = Rcpp::as<std::string>(r_.parseEval("geterrmessage()"));
                throw RException(message);
            }
            catch(...) {
                throw RException("Unknown exception");
            }
        #else
            Rcpp::Function func = context_.get(name);
            Rcpp::Language call(func,args...);
            return call.eval();
        #endif
    }

    template<
        typename Result,
        typename... Args
    >
    Result call_(const char* name,Args... args){
        SEXP result = call_(name,args...);
        // Currently, this function only handles strings returned from R and then casts those
        // using boost::lexical_cast. I got serious errors of the form:
        //    memory access violation at address: 0x7fff712beff8: no mapping at fault address
        // when trying to use Rcpp::as<bool> or Rcpp::as<int> even when checking the returned SEXP was
        // the correct type
        if(TYPEOF(result)!=STRSXP) STENCILA_THROW(Exception,"R-side methods should return a string");
        return unstring<Result>(Rcpp::as<std::string>(result));
    }

public:
    
    #ifdef STENCILA_R_EMBED
        RContext(void){
            // Execute implementation code
            static bool initialised_ = false;
            if(not initialised_){
                r_.parseEvalQ(code_());
                initialised_ = true;
            }
            // Create a context
            id_ = "context"+boost::lexical_cast<std::string>(contexts_++);
            r_.parseEvalQ(id_ + " <- Context('.')");
        }
    #else
        RContext(SEXP sexp){
            context_ = Rcpp::Environment(sexp);
        }
    #endif

    virtual ~RContext(void){}

    std::string details(void) const {
        return "RContext at " + string(this);
    };

    bool accept(const std::string& language) const {
        return language=="r";
    }

    std::string execute(const std::string& code, const std::string& id="", const std::string& format="", const std::string& width="", const std::string& height="", const std::string& units=""){
        return call_<std::string>("execute",code,id,format,width,height,units);
    }

    std::string interact(const std::string& code, const std::string& id=""){
        return call_<std::string>("interact",code,id);
    }

    void assign(const std::string& name, const std::string& expression){
        call_("assign",name,expression);
    }

    void input(const std::string& name, const std::string& type, const std::string& value){
        call_("input",name,type,value);
    };

    std::string write(const std::string& expression){
        return call_<std::string>("write",expression);
    }
    
    bool test(const std::string& expression){
        return call_<bool>("test",expression);
    }

    void mark(const std::string& expression){
        call_("mark",expression);
    }

    bool match(const std::string& expression){
        return call_<bool>("match",expression);
    }

    void unmark(void){
        call_("unmark");
    }

    bool begin(const std::string& item,const std::string& items){
        return call_<bool>("begin",item,items);
    }

    bool next(void){
        return call_<bool>("next_");
    }

    void enter(const std::string& expression){
        call_("enter",expression);
    }

    void enter(void){
        call_("enter");
    }

    void exit(void){
        call_("exit");
    }
};

}
