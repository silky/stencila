#pragma once

#include <stack>
#include <string>

#include <stencila/component.hpp>

namespace Stencila {

class Context : public Component {
public:

	// Virtual destructor to prevent compiler warnings
	virtual ~Context(void) {};

	/**
	 * Get details of this context
	 */
	virtual std::string details(void) const = 0;

	/**
	 * Does this context support the given language
	 * 
	 * @param  language A language code (e.g. "py","r","js")
	 */
	virtual bool accept(const std::string& language) const = 0;

	/**
	 * Execute code within the context
	 * 
	 * @param code String of code
	 * @param id Unique identifier for this execution
	 * @param format Output format
	 * @param width Width of output (image formats only)
	 * @param height Height of output (image formats only)
	 * @param units Units of width and height (image formats only)
	 */
	virtual std::string execute(const std::string& code, const std::string& id="", const std::string& format="", const std::string& width="", const std::string& height="", const std::string& units="") = 0;
	
	/**
	 * Execute a peice of code and return an interactive result
	 *
	 * This method is used for allowing contexts to be use in a 
	 * [read-eval-print loop](http://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop).
	 * 
	 * @param  code String of code
	 * @param  id   Unique identifier for this execution
	 * @return      String representation of the result of executing the code
	 */
	virtual std::string interact(const std::string& code, const std::string& id="") = 0;

	/**
	 * Assign an expression to a name.
	 * Used by stencil `import` and `include` elements to assign values
	 * to the context of the transcluded stencils.
	 * 
	 * @param name       Name to be assigned
	 * @param expression Expression to be assigned to name
	 */
	virtual void assign(const std::string& name, const std::string& expression) = 0;

	/**
	 * Apply user input to the context
	 * Used by stencil `<input>` elements.
	 * 
	 * @param name  Name of the input
	 * @param type  Type of the input (e.g. "text","number")
	 * @param value Value of the input
	 */
	virtual void input(const std::string& name, const std::string& type, const std::string& value) = 0;

	/**
	 * Get a text representation of an expression. 
	 * Used by stencil `text` elements e.g. `<span data-text="x">42</span>`
	 * 
	 * @param  expression Expression to convert to text
	 */
	virtual std::string write(const std::string& expression) = 0;

	/**
	 * Test whether an expression is true or false. 
	 * Used by stencil `if` elements e.g. `<span data-if="height>10">The height is greater than 10</span>`
	 * 
	 * @param  expression Expression to evaluate
	 */
	virtual bool test(const std::string& expression) = 0;

	/**
	 * Mark an expression to be the subject of subsequent `match` queries.
	 * Used by stencil `switch` elements e.g. `<p data-switch="x"> X is <span data-match="1">one</span><span data-default>not one</span>.</p>`
	 * 
	 * @param expression Expression to evaluate
	 */
	virtual void mark(const std::string& expression) = 0;

	/**
	 * Test whether an expression matches the current subject.
	 * Used by stencil `match` elements (placed within `switch` elements)
	 * 
	 * @param  expression Expression to evaluate
	 */
	virtual bool match(const std::string& expression) = 0;

	/**
	 * Unmark the current subject expression
	 */
	virtual void unmark(void) = 0;
	
	/**
	 * Begin a loop.
	 * Used by stencil `for` elements e.g. `<ul data-for="planet:planets"><li data-each data-text="planet" /></ul>`
	 * 
	 * @param  item  Name given to each item
	 * @param  expression Expression giveing an iterable list of items
	 */
	virtual bool begin(const std::string& item,const std::string& expression) = 0;

	/**
	 * Steps the current loop to the next item. 
	 * Used by stencil `for` elements. See stencil `render`ing methods.
	 *
	 * If there are more items to iterate over this method should return `true`.
	 * When there are no more items, this method should do any clean up required 
	 * (e.g popping the loop namespace off a namespace stack) when ending a loop, 
	 * and return `false`. 
	 */
	virtual bool next(void) = 0;

	/**
	 * Enter a new namespace. 
	 * Used by stencil `with` element e.g. `<div data-with="mydata"><span data-text="sum(a*b)" /></div>`
	 *  
	 * @param expression Expression that will be the scope of the new context
	 */
	virtual void enter(const std::string& expression="") = 0;

	/**
	 * Exit the current namespace
	 */
	virtual void exit(void) = 0;
	
protected:

	/**
	 * Method to throw an "unsupported" exception
	 */
	void unsupported_(const std::string& method){
		throw Exception("Method \"" + method + "\" not supported by this type of context");
	}
};

}
