#include <stencila/sheet.hpp>
using namespace Stencila;

#include "stencila.hpp"
#include "spread.hpp"

STENCILA_R_NEW(Sheet)

STENCILA_R_EXEC1(Sheet,initialise,std::string)

STENCILA_R_EXEC2(Sheet,load,std::string,std::string)
STENCILA_R_RET1(Sheet,dump,std::string)

STENCILA_R_EXEC1(Sheet,import,std::string)
STENCILA_R_FUNC Sheet_export(SEXP self,SEXP path){
	// Need to wrap the `export` method manually
	// because `export` is a keyword in C++
    STENCILA_R_BEGIN
        from<Sheet>(self).export_(as<std::string>(path));
        return null;
    STENCILA_R_END
}

STENCILA_R_EXEC1(Sheet,graphviz,std::string)

STENCILA_R_EXEC1(Sheet,read,std::string)
STENCILA_R_EXEC1(Sheet,write,std::string)

STENCILA_R_EXEC0(Sheet,store)
STENCILA_R_EXEC0(Sheet,restore)

STENCILA_R_EXEC0(Sheet,compile)

STENCILA_R_RET0(Sheet,serve) 
STENCILA_R_EXEC0(Sheet,view)
STENCILA_R_GET(Sheet,page)
STENCILA_R_EXEC1(Sheet,page,std::string)

STENCILA_R_FUNC Sheet_attach(SEXP self,SEXP spread){
    // Need to use `make_shared` here 
    STENCILA_R_BEGIN
        from<Sheet>(self).attach(std::make_shared<RSpread>(spread));
        return null;
    STENCILA_R_END
}
STENCILA_R_EXEC0(Sheet,detach)

STENCILA_R_EXEC0(Sheet,update)

STENCILA_R_FUNC Sheet_test(SEXP self){
    STENCILA_R_BEGIN
        return wrap(
            from<Sheet>(self).test().dump()
        );
    STENCILA_R_END
}

STENCILA_R_RET0(Sheet,list)

STENCILA_R_RET1(Sheet,content,std::string)
