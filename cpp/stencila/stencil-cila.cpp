#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>
#include <boost/lexical_cast.hpp>

#include <stencila/stencil.hpp>
#include <stencila/html.hpp>
#include <stencila/string.hpp>

namespace Stencila {

class CilaParser {
public:
	typedef Stencil::Node Node;

	/**
	 * Alternative parsing states
	 */
	enum State {
		/**
		 * Start of line state
		 */
		sol,

		/**
		 * Looking for element
		 *
		 * In this state can move across into `attrs`
		 */
		elem,

		/**
		 * Looking for element attributes
		 *
		 * In this state the parser is looking for HTML element attribute
		 * syntax (e.g. `[id="an-id"]`, `#an-id`, `.a-class`) including directives (e.g. `text x`) 
		 * and ignoring whitepace. If no attribute is found then moves across to `text` state.
		 */
		attrs,

		/**
		 * Looking for rendering flags (e.g. hash, index, off) some of which are only applied to
		 * directives:
		 *   - hash
		 *   - off
		 * and others which can be applied to both directives and normal elements
		 *   - index
		 *   - lock
		 *   - included 
		 */
		flags,

		/**
		 * Text including shorthands and ilined elements
		 */
		text,

		/**
		 * Within an emphasis section (e.g `_this text is emphasised_`)
		 */
		empha,

		/**
		 * Within a strong section (e.g `*this text is strong*`)
		 */
		strong,

		/**
		 * Within an interpolation section (e.g ``answer``)
		 */
		interp,		

		/**
		 * Within a code section (e.g `answer = 42`)
		 */
		code,

		/**
		 * Within an AsciiMath section (e.g `|e = mc^2|`)
		 */
		asciimath,

		/**
		 * Within a TeX/LaTeX section (e.g `\(e = mc^2\)`)
		 */
		tex,

		/**
		 * Within an `exec` a `style` directive. Embedded code.
		 */
		embed
	};

	/**
	 * Current state
	 */
	State state;

	/**
	 * State stack.
	 *
	 * Allows for nesting of parsing states. For example, `strong` within `empha`.
	 */
	std::deque<State> states;

	/**
	 * Beggining of input
	 */
	std::string::const_iterator begin;

	/**
	 * End of input
	 */
	std::string::const_iterator end;

	/**
	 * Current regex match
	 *
	 * Used to obtain results from the `is` method
	 */
	boost::smatch match;

	/**
	 * Stencil being parse into
	 */
	Node stencil;

	/**
	 * Current indentation. Used for keeping track
	 * of parent-child relationships
	 */
	std::string indent;

	/**
	 * Current HTML node
	 */
	Node node;

	/**
	 * Stack of nodes for enter/exit
	 */
	struct Element {
		std::string indent;
		Node node;
	};
	std::deque<Element> nodes;

	/**
	 * Buffer of characters to be added as HTML text
	 */
	std::string buffer;

	/**
	 * Bilge of characters which may me kept or discarded for
	 * embedded code elements
	 */
	std::string bilge;

	/**
	 * Flag for orphaned element attributes
	 */
	bool tag_needed;

	/**
	 * Flag for a paragraph is needed
	 */
	bool para_needed;

	/**
	 * Get string representation of a state for debugging
	 */
	std::string state_name(State state) const {
		switch(state){
			#define CASE(STATE) case STATE: return #STATE;
			CASE(sol)
			CASE(elem)
			CASE(attrs)
			CASE(flags)
			CASE(text)
			CASE(empha)
			CASE(strong)
			CASE(code)
			CASE(asciimath)
			CASE(tex)
			CASE(embed)
			#undef CASE
			default: return "";
		}
	}

	/**
	 * Push into a parsing state
	 */
	void push(State to){
		states.push_back(to);
		state = to;
	}

	/**
	 * Pop out of a parsing state
	 */
	void pop(void){
		if(states.size()<2){
			#if defined(STENCILA_CILA_PARSER_TRACE)
				trace_show();
			#endif
			throw std::runtime_error("Too few states to pop: "+boost::lexical_cast<std::string>(states.size()));
		}
		states.pop_back();
		state = states.back();
	}

	/**
	 * Move across into another parsing state
	 */
	void across(State to){
		if(states.size()>0) states.pop_back();
		states.push_back(to);
		state = to;
	}

	/**
	 * Add a character to buffer
	 */
	void add(char cha){
		buffer += cha;
	}

	/**
	 * Add characters to buffer
	 */
	void add(const std::string& chars){
		buffer += chars;
	}

	/**
	 * Add a character to buffer from input
	 */
	void add(void){
		if(begin!=end){
			buffer.insert(buffer.end(),begin,begin+1);
			begin += 1;
		}
	}

	/**
	 * Flush the buffer to the current HTML elment
	 * as a text node
	 */
	Node flush(void){
		Node text;
		if(buffer.length()) text = node.append_text(buffer);
		buffer = "";
		return text;
	}

	/**
	 * Enter a HTML element
	 *
	 * When using this method you are responsible for calling `flush()` first!
	 * 
	 * @param elem Element to enter
	 */
	void enter(Node elem){
		node = elem;
		nodes.push_back({indent,node});
		tag_needed = false;
		para_needed = false;
	}

	/**
	 * Enter a HTML element with given tag name
	 */
	void enter(const std::string& name){
		flush();

		node = node.append(name);
		nodes.push_back({indent,node});
		tag_needed = false;
		para_needed = false;
	}

	/**
	 * Exit a HTML element
	 */
	void exit(){
		flush();

		if(nodes.size()) nodes.pop_back();
		if(nodes.size()) node = nodes.back().node;
		else node = stencil;
	}

	/**
	 * Enter an element and push into a state
	 */
	void enter_push(const std::string& name,State to){
		enter(name);
		push(to);
	}

	/**
	 * Enter an element and move across into a state
	 */
	void enter_across(const std::string& name,State to){
		enter(name);
		across(to);
	}

	/**
	 * Exit an element and pop out of a state
	 */
	void exit_pop(){
		exit();
		pop();
	}

	/**
	 * Check for a regular expression match at the start 
	 * of input buffer
	 * 
	 * @param  regex Regex to find
	 * @return       Was the regex matched?
	 */
	bool is(const boost::regex& regex){
		//match_continuous specifies that the expression must match a sub-sequence that begins at first.
		bool found = boost::regex_search(begin, end, match, regex, boost::regex_constants::match_continuous);
		if(found) begin += match.position() + match.length();
		return found;
	}


#if defined(STENCILA_CILA_PARSER_TRACE)

	// Tracing of state changes used in debugging

	struct Trace {
		State state;
		int states = -1;
		int nodes = -1;
		std::string begin;
		std::string regex = "<?>";
		std::string match = "<?>";
	};
	std::vector<Trace> traces;

	void trace_begin(void){
		traces.clear();
	}

	void trace_new(void){
		Trace current;
		current.state = state;
		current.states = states.size();
		current.nodes = nodes.size();
		current.begin = *begin;
		boost::replace_all(current.begin,"\t","\\t");
		boost::replace_all(current.begin,"\n","\\n");
		boost::replace_all(current.begin," ","\\s");
		traces.push_back(current);
	}

	void trace(const char* regex){
		Trace& current = traces[traces.size()-1];
		current.regex = regex;
		std::string str = "<none>";
		if(not match.empty()){
			try {
				str = match.str();
			}
			catch(const std::logic_error&){
			}
			boost::replace_all(str,"\t","\\t");
			boost::replace_all(str,"\n","\\n");
			boost::replace_all(str," ","\\s");
		}
		current.match = str;
	}

	void trace_show(void) const {
		std::cout<<"-------------------Trace--------------------------------\n";
		std::cout<<"state\tstates\tnodes\tbegin\tregex\t\tmatch\n";
		std::cout<<"--------------------------------------------------------\n";
		for(auto item : traces){
			std::cout<<state_name(item.state)<<"\t"<<item.states<<"\t"<<item.nodes<<"\t"<<item.begin<<"\t"
					<<item.regex<<"\t\t"<<item.match<<"\n";
		}
		std::cout<<"--------------------------------------------------------\n";
		
	}

#else

	// When tracing is off these methods do nothing
	void trace_begin(void){}
	void trace_new(void){}
	void trace(const char* regex){}
	void trace_show(void) const {}

#endif

	/**
	 * Parse a string of Cila
	 * 
	 * @param cila Um, cila to parse
	 */
	CilaParser& parse(const std::string& cila){
		// Initialise members...
		// ... input
		begin = cila.cbegin();
		end = cila.cend();
		// ... states
		states.clear();
		states.push_back(sol);
		state = states.back();
		// ... stencil
		stencil.clear();
		// ... nodes
		nodes.clear();
		nodes.push_back({"",stencil});
		node = nodes.back().node;

		tag_needed = false;

		// Plain text at the start will get treated as a paragraph
		// (subsequently needs to have a blank line before it)
		para_needed = true;

		// Define regular expressions
		static const boost::regex
			indentation("[ \\t]*"),

			// Not necessary all tags, just those that are valid in stencils
			tag("("\
				"section|nav|article|aside|address|h1|h2|h3|h4|h5|h6|p|hr|pre|blockquote|ol|ul|li|dl|dt|dd|" \
				"figure|figcaption|div|a|em|strong|small|s|cite|q|dfn|abbr|data|time|code|var|samp|kbd|sub|sup|i|b|u|mark|ruby|" \
				"rt|rp|bdi|bdo|span|br|wbr|ins|del|table|caption|colgroup|col|tbody|thead|tfoot|tr|td|th|" \
				// Image elements
				"img|svg|" \
				// Form elements
				"form|fieldset|label|input|select|textarea|button"
			")\\b"),

			section(">\\s*([ \\w-]+)"),

			ul_item("-\\s*"),
			ol_item("\\d+\\.\\s*"),

			attr("\\[([\\w-]+)=(.+?)\\]"),
			id("#([\\w-]+)\\b"),
			clas("\\.([\\w-]+)\\b"),
			
			exec_open("(exec|js|r|py)\\b(.*?)(?=( ~ )|\\n|$)"),
			out("out"),
			
			style_open("(style|css)\\b(\\n|$)"),

			// Spaces around semicolons and colons are used to preven confusion with those
			// symbols in expressions
			directive_noarg("(each|else|default)\\b *(?=( ~ )|( : )|\\n|\\{|\\}|$)"),
			directive_arg_optional("(comments)\\b(.*?)(?=( ~ )|( : )|\\n|\\{|\\}|$)"),
			directive_arg("(when|refer|attr|text|icon|with|if|elif|switch|case|for|include|delete|replace|change|before|after|prepend|append|macro|par|set|comment)\\b(.+?)(?=( ~ )|( : )|\\n|\\{|\\}|$)"),

			// Just used for eting up spaces between attributes and flags
			spaces(" +"),
			
			// Indicates the directive arguments have ended
			// Use tilde because it is generally quite rare in language expressions. Use
			// surrounding spaces to prevent ambiguity with any qithin expression tilde.
			flags_open(" ~ "),
			// Flags
			// These are placed after all directive arguments so that the layout of stencil logic (i.e. directive names and args)
			// is not affected when they are added. i.e. they should be at the end of directive lines.
			hash("&([a-zA-Z0-9]+)"),
			index("\\^(\\d+)"),
			error  ("\\!\\\"([^\\\"]*)\\\"(\\@(\\d+(,\\d+)?))?"),
			warning("\\%\\\"([^\\\"]*)\\\"(\\@(\\d+(,\\d+)?))?"),
			lock("lock"),
			off("off"),
			included("included"),

			// Indicates the directive arguments and flags have ended
			// Needed to separate those from any content that may be following in inline elements
			// (for block elements a newline does the separation)
			// Use surrounding spaces to prevent ambiguity with in expression spaces
			directive_close(" : "),

			empha_open("(\\s)_(?=[^\\s])"),
			empha_close("_"),
			strong_open("(\\s)\\*(?=[^\\s])"),
			strong_close("\\*"),

			backtick_escaped("\\\\`"),
			backtick("`"),

			pipe_escaped("\\\\\\|"),
			pipe("\\|"),

			tex_open("\\\\\\("),
			tex_close("\\\\\\)"),

			link("(\\[)([^\\]]*)(\\]\\()([^\\)]+)(\\))"),
			autolink("\\bhttp(s)?://[^ ]+\\b"),
			autoemail("[a-zA-Z0-9_-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9]+"),

			at_escaped("\\\\@"),
			refer("@([\\w-]+)\\b"),

			curly_open("\\{"),
			curly_close("\\}"),

			blankline("[ \\t]*\\n"),
			endline("\\n"),
			endinput("$")
		;

		trace_begin();
		while(begin!=end){
			trace_new();

			if(state==sol){
				// If this is not a blank line (zero or more spaces or tabs and nothing else)
				if(not boost::regex_search(begin, end, blankline, boost::regex_constants::match_continuous)){
					// Get indentation
					is(indentation);
					indent = match.str();
					// Peek ahead to see if this is a `li` shorthand line; for these
					// we don't want to pop off the parent `ul` or `ol`
					bool ul_li = boost::regex_search(begin, end, ul_item, boost::regex_constants::match_continuous);
					bool ol_li = boost::regex_search(begin, end, ol_item, boost::regex_constants::match_continuous);
					// Exit nodes until a node with lower indentation is reached
					// which then becomes the current node to which others get appended
					auto line_indent = indent.size();
					while(nodes.size()>1 and 
						(nodes.back().indent=="none" or line_indent<=nodes.back().indent.size())
					){
						auto node_indent = nodes.back().indent.size();
						if(ul_li and node.name()=="ul" and indent.size()==node_indent) break;
						if(ol_li and node.name()=="ol" and indent.size()==node_indent) break;
						exit();
					}
				}

				if(is(exec_open)){
					trace("exec");
					// An execute directive should only begin at the 
					// start of a line
					// Enter `<pre>` element and move across to `embed` state;
					enter_across("pre",embed);
					std::string arg = match[1].str();
					std::string rest = trim(match[2].str());
					if(rest.length()) arg += " " + rest;
					node.attr("data-exec",arg);
				}
				else if(is(out)){
					trace("out");
					// Output from an execute directive
					// No attibutes should follow but to eat up spaces before
					// child elements, go to attributes
					enter_across("div",attrs);
					node.attr("data-out","true");
				}
				else if(is(style_open)){
					trace("style");
					// A style directive should only begin at the 
					// start of a line
					// Enter `<style>` element and move across to `embed` state;
					enter_across("style",embed);
					std::string type = "text/css";
					node.attr("type",type);
					add("\n");
				}
				else if(is(blankline)){
					trace("blank");
					para_needed = true;
				}
				else {
					trace("other");
					// Move across into elem state
					across(elem);
				}
			}
			else if(state==elem){
				// Attempt to match...
				if(is(tag)){
					trace("tag");
					// Enter new element and move to `attrs` state to 
					// start looking for attributes
					enter_across(match.str(),attrs);
				}
				else if(is(section)){
					trace("section");
					// Enter `<section>` move into `elem` state to allow
					// for any further attributes
					flush();
					auto id = match[1].str();
					boost::to_lower(id);
					boost::replace_all(id," ","-");
					auto section = node.append("section").attr("id",id);
					auto title = match[1].str();
					auto h1 = section.append("h1").text(title);
					enter(section);
					across(elem);
				}
				else if(is(ul_item)){
					trace("ul_item");
					// Enter `<ul>` if necessary, enter `<li>` and move into `text` state
					if(node.name()!="ul") enter("ul");
					enter("li");
					across(text);
				}
				else if(is(ol_item)){
					trace("ol_item");
					// Enter `<ol>` if necessary, enter `<li>` and move into `text` state
					if(node.name()!="ol") enter("ol");
					enter("li");
					across(text);
				}
				else if(is(pipe)){
					trace("pipe");
					// Enter `<script>` and push into `asciimath` state
					flush();
					auto span = node.append("p",{{"class","equation"}});
					auto script = span.append("script",{{"type","math/asciimath; mode=display"}});
					enter(script);
					push(asciimath);
				}
				else if(is(tex_open)){
					trace("tex_open");
					// Enter `<script>` and push into `tex` state
					flush();
					auto span = node.append("p",{{"class","equation"}});
					auto script = span.append("script",{{"type","math/tex; mode=display"}});
					enter(script);
					push(tex);
				}
				else{
					trace("none");
					// Indicate that a new element is required
					// for any subsequent attributes
					tag_needed = true;
					// Move across to `attrs` state to look for any attributes
					across(attrs);
				}
			}
			else if(state==attrs){
				// Local lambda for entering a new element if needed
				auto enter_elem_if_needed = [this](const std::string& name="div"){
					if(tag_needed) enter(name);
				};
				// Attempt to match...
				if(is(attr)){
					trace("attr");
					// Enter new element if necessary and create attribute;
					// keep on looking for more attributes
					enter_elem_if_needed();
					node.attr(match[1].str(),match[2].str());
				}
				else if(is(id)){
					trace("id");
					// Enter new element if necessary and create id attribute;
					// keep on looking for more attributes
					enter_elem_if_needed();
					node.attr("id",match[1].str());
				}
				else if(is(clas)){
					trace("clas");
					// Enter new element if necessary and create class attribute;
					// keep on looking for more attributes
					enter_elem_if_needed();
					node.concat("class",match[1].str());
				}
				else if(is(directive_noarg)){
					trace("directive_noarg");
					// Enter new element if necessary and create directive attribute;
					enter_elem_if_needed();
					node.attr("data-"+match[1].str(),"true");
				}
				else if(is(directive_arg_optional)){
					trace("directive_arg_optional");
					// Enter new element if necessary and create directive attribute;
					auto directive = match[1].str();
					auto arg = trim(match[2].str());
					enter_elem_if_needed();
					node.attr("data-"+directive,arg);
				}
				else if(is(directive_arg)){
					trace("directive_arg");
					// Enter new element if necessary and create directive attribute;
					// type of element depends on which directive;
					// move across to `flags` state (i.e no attributes or text to follow)
					auto directive = match[1].str();
					std::string arg = trim(match[2].str());
					if(directive=="text" or directive=="refer") enter_elem_if_needed("span");
					else enter_elem_if_needed();
					node.attr("data-"+directive,arg);
				}
				else if(is(flags_open)){
					trace("flags");
					enter_elem_if_needed();
					across(flags);
				}
				else if(is(directive_close)){
					trace("directive_close");
					across(text);
				}
				else if(is(spaces)){
					trace("spaces");
					// Ignore spaces and keep on looking for attributes
				}
				else {
					trace("none");
					// If no match move across to `text`
					across(text);
				}
			}
			else if(state==flags){
				if(is(hash)){
					trace("hash");
					node.attr("data-hash",match[1].str());
				}
				else if(is(index)){
					trace("index");
					node.attr("data-index",match[1].str());
				}
				else if(is(error)){
					trace("error");
					auto value = match[1].str();
					if(match[3].str()!="") value += "@"+match[3].str();
					node.attr("data-error",value);
				}
				else if(is(warning)){
					trace("warning");
					auto value = match[1].str();
					if(match[3].str()!="") value += "@"+match[3].str();
					node.attr("data-warning",value);
				}
				else if(is(lock)){
					trace("lock");
					node.attr("data-lock","true");
				}
				else if(is(off)){
					trace("off");
					node.attr("data-off","true");
				}
				else if(is(included)){
					trace("included");
					node.attr("data-included","true");
				}
				else if(is(directive_close)){
					trace("directive_close");
					across(text);
				}
				else if(is(spaces)){
					trace("spaces");
					// Ignore spaces and keep on looking for flags
				}
				else {
					trace("none");
					// If current state is under an `embed` state then 
					// pop up to the `embed` otherwise move across to `text`
					bool under_embed = false;
					if(states.size()>1){
						if(states[states.size()-2]==embed) under_embed = true;
					}
					if(under_embed){
						pop();
					} else {
						across(text);
					}
				}
			}
			else if(state==text){
				// Enter a new paragraph if necessary
				if(para_needed) enter("p");
				// Any elements that are `enter()`ed from here on
				// will be inlines so set indent to none.
				indent = "none";
				// Attempt to match...
				if(is(curly_open)){
					trace("curly_open");
					// Push into `elem` state
					push(elem);
				}
				else if(is(curly_close)){
					trace("curly_close");
					// Exit from current element and pop out of `text` state
					exit_pop();
				}
				else if(is(empha_open)){
					trace("empha_open");
					// Add captured preceeding whitespace
					add(match[1].str());
					// Enter `<em>` and push into `empha` state
					enter_push("em",empha);
				}
				else if(is(strong_open)){
					trace("strong_open");
					// Add captured preceeding whitespace
					add(match[1].str());
					// Enter `<strong>` and push into `strong` state
					enter_push("strong",strong);
				}
				else if(is(backtick_escaped)){
					trace("backtick_escaped");
					// Replace with backtick
					add('`');
				}
				else if(is(backtick)){
					trace("backtick");
					// Enter `<code>` and push into `code` state
					enter_push("code",code);
				}
				else if(is(pipe_escaped)){
					trace("pipe_escaped");
					// Replace with pipe
					add('|');
				}
				else if(is(pipe)){
					trace("pipe");
					// Enter `<script>` and push into `asciimath` state
					flush();
					auto script = node.append("script",{{"type","math/asciimath"}});
					enter(script);
					push(asciimath);
				}
				else if(is(tex_open)){
					trace("tex_open");
					// Enter `<script>` and push into `tex` state
					flush();
					auto script = node.append("script",{{"type","math/tex"}});
					enter(script);
					push(tex);
				}
				else if(is(link)){
					trace("link");
					// Flush text and append `<a>`
					flush();
					node.append("a").attr("href",match[4].str()).text(match[2].str());
				}
				else if(is(autolink)){
					trace("autolink");
					// Flush text and append `<a>`
					flush();
					node.append("a").attr("href",match.str()).text(match.str());
				}
				else if(is(autoemail)){
					trace("autoemail");
					// Needs to be before `refer` to prevent @ begin matched there
					// Flush text and append a mailto link
					flush();
					node.append("a").attr("href","mailto:"+match.str()).text(match.str());
				}
				else if(is(at_escaped)){
					trace("at_escaped");
					// Replace with at
					add('@');
				}
				else if(is(refer)){
					trace("refer");
					// Flush text and append `<span data-refer="#id" />`
					flush();
					node.append("span").attr("data-refer","#"+match[1].str());
				}
				else if(is(endline)){
					trace("endline");
					// Move across into `sol` state
					across(sol);
				}
				else {
					trace("other");
					// Add character to buffer
					add();
				}
			}
			else if(state==empha){
				if(is(empha_close)) exit_pop();
				else if(is(strong_open)){
					add(match[1].str());
					enter_push("strong",strong);
				}
				else add();
			}
			else if(state==strong){
				if(is(strong_close)) exit_pop();
				else if(is(empha_open)) {
					add(match[1].str());
					enter_push("em",empha);
				}
				else add();
			}
			else if(state==code){
				if(is(backtick_escaped)) add('`');
				else if(is(backtick)) exit_pop();
				else add();
			}
			else if(state==asciimath){
				if(is(pipe_escaped)) add('|');
				else if(is(pipe)) exit_pop();
				else add();
			}
			else if(state==tex){
				if(is(tex_close)) exit_pop();
				else add();
			}
			else if(state==embed){
				// Capture all characters but on new lines
				// move to `sol` state to see if indentation
				// has reduced and should pop out of this state
				// @todo Remove leading indentation
				if(is(flags_open)){
					trace("flags");
					push(flags);
				}
				else {
					static const boost::regex line("([ \t]*)([^\n]*)(\n|$)");
					boost::smatch match_local;
					boost::regex_search(begin, end, match_local, line, boost::regex_constants::match_continuous);
					auto indent_line = match_local[1].str();
					auto content_line = match_local[2].str();
					// Should this `embed` state end?
					if(content_line.length()>0 and indent_line.length()<=indent.length()){
						// Exit and pop. Note that `begin` is not shifted along at all
						// so that the line can be processed by `sol`
						exit();
						across(sol);
					} else {
						if(content_line.length()==0){
							// If this is an empty or blank (only whitespace chars) line then add a newline to the bilge
							// This means that whitespace chars on a blank line are considered insignificant; they are discarded
							bilge += "\n";
						} else {
							// Line is not empy, so use any bilge and add line to buffer
							// Add bilge to buffer and clear it
							buffer += bilge;
							bilge = "";
							// Add line to buffer
							if(indent_line.length()>=indent.length()+1) buffer += indent_line.substr(indent.length()+1);
							buffer += content_line;
							buffer += "\n";
						}
						// Shift along
						begin += match_local.position() + match_local.length();
					}
				}
			}
			else add();
		}

		// Flush any remaining beffer to the current element
		flush();

		return *this;
	}

	CilaParser& parse(Node node,const std::string& cila){
		stencil = node;
		node.clear();
		return parse(cila+"\n");
	}
};

//#define STENCILA_CILA_GENERATOR_TRACE
class CilaGenerator {
public:
	typedef Stencil::Node Node;
	typedef Stencil::Nodes Nodes;

	/**
	 * Generated Cila
	 */
	std::stringstream cila;

	uint newlines = 0;

	/**
	 * Add line context
	 */
	void content(const std::string& content){
		if(content.length()>0){
			for(auto i=content.begin(); i!=content.end(); ++i){
				if(*i!='\n'){
					newlines = 0;
					break;
				}
			}
			for(auto i=content.rbegin(); i!=content.rend(); ++i){
				if(*i=='\n') newlines++;
				else break;
			}
			cila<<content;
		}
	}

	/**
	 * Start a new line
	 */
	void newline(const std::string& indentation=""){
		if(newlines<1){
			cila<<"\n";
			newlines++;
		}
		cila<<indentation;
	}

	/**
	 * Ensure a blank line
	 */
	void blankline(void){
		while(newlines<2){
			cila<<"\n";
			newlines++;
		}
	}

	void generate_node(Node node, const std::string& indent=""){
		if(node.is_document()){
			// Generate children with no indentation
			generate_children(node.children(),true);
		}
		else if(node.is_element()){
			auto name = node.name();
			auto children_list = node.children();
			auto children = children_list.size();
			auto attribute_list = node.attrs();
			auto attributes = attribute_list.size();

			// Remove an attribute already dealt with
			auto erase_attr = [&](const std::string& attr){
				attribute_list.erase(
					std::remove(attribute_list.begin(),attribute_list.end(),attr),
				attribute_list.end());
			};

			// Shorthands from whence we return...if we don't then the
			// default generation happens (that's why it's not an if,else if tree)
			
			// Refer directive shorthand
			if(name=="span" and children==0 and attributes==1 and node.attr("data-refer").length()){
				auto value = node.attr("data-refer");
				if(value[0]=='#'){
					int spaces = std::count_if(value.begin(), value.end(),[](unsigned char c){ return std::isspace(c); });
					if(spaces==0){
						content("@"+value.substr(1));
						return;
					}
				}
			}
			// Emphasis & strong
			if((name=="em" or name=="strong") and attributes==0){
				std::string delim;
				if(name=="em") delim = "_";
				else delim = "*";
				content(delim);
				generate_children(children_list);
				content(delim);
				return;
			}
			// Code
			if(name=="code" and attributes==0){
				auto text = node.text();
				boost::replace_all(text,"`","\\`");
				content("`"+text+"`");
				return;
			}
			// Equations and inline math
			if(name=="script" and node.attr("type")=="math/asciimath"){
				auto code = trim(node.text());
				boost::replace_all(code,"|","\\|");
				content('|'+code+'|');
				return;
			}
			if(name=="script" and node.attr("type")=="math/tex"){
				auto code = trim(node.text());
				content("\\("+code+"\\)");
				return;
			}
			// Links, autolinks and autoemails
			if(name=="a" and attributes==1 and node.has("href")){
				auto text = node.text();
				auto href = node.attr("href");
				if(text==href) content(text);
				else if(href.substr(0,7)=="mailto:" and href.substr(7)==text) content(text);
				else content("["+text+"]("+href+")");
				return;
			}

			// Lists with no attributes and children with no attributes
			if((name=="ul" or name=="ol") and attributes==0 and children>0){
				// Check all of the children can be represented by a dash ("-")
				// i.e. they have no attributes
				bool all_ok = true;
				for(Node child : children_list){
					if(child.attrs().size()>0){
						all_ok = false;
						break;
					}
				}
				if(all_ok){
					blankline();
					bool ol = name=="ol";
					int index = 0;
					for(auto li : children_list){
						newline(indent);
						index++;
						if(ol) content(string(index)+". ");
						else content("- ");
						generate_children(li.children(),false,indent+"\t");
					}
					blankline();
					return;
				}
			}
			// Plain paragraph
			if(name=="p" and children>0 and attributes==0){
				// ... and only inline-able children
				bool shorthandable = true;
				for(Node child : children_list){
					if(not (child.is_text() or Html::is_inline_element(child))){
						shorthandable = false;
					}
				}
				if(shorthandable){
					blankline();
					// Indent the start of this paragraph
					newline(indent);
					generate_children(children_list);
					blankline();
					return;
				}
			}
			// Equation paragraph
			if(name=="p" and node.attr("class")=="equation"){
				auto script = node.select("script");
				if(script){
					auto type = script.attr("type");
					if(type.length()){
						auto code = trim(script.text());
						std::string begin,end;
						if(type.find("math/asciimath")!=std::string::npos){
							begin = "|";
							end = "|";
							boost::replace_all(code,"|","\\|");
						} else {
							begin = "\\(";
							end = "\\)";
						}
						
						blankline();
						newline(indent);
						content(begin+code+end);
						blankline();
						return;
					}
				}
			}
			// Sections with an id attribute and a <h1> child
			if(name=="section" and node.attr("id").length() and children>0){
				// Only proceed if <h1> is first child
				if(children_list[0].name()=="h1"){
					// Only proceed if id is consistent with header
					auto h1 = node.select("h1");
					auto title = h1.text();
					auto id_expected = title;
					boost::trim(id_expected);
					boost::to_lower(id_expected);
					boost::replace_all(id_expected," ","-");
					auto id = node.attr("id");
					if(id==id_expected){
						// Add shorthand with blank line before
						blankline();
						newline(indent);
						content("> "+boost::trim_copy(title));
						// Generate each child on a new line except for the h1
						Nodes children_to_generate;
						for(Node child : children_list){
							if(not(child.name()=="h1" and child.text()==title)){
								children_to_generate.push_back(child);
							}
						}
						generate_children(children_to_generate,true,indent+"\t");
						return;
					}
				}
			}

			// Everything that could not be shorthanded still remains here...
		
			// Is this an inline element?
			bool inline_element  = Html::is_inline_element(name);

			// If a block element, does this element have embedded 
			// code content (ie. exec or style)?
			bool embedded = false;

			// If a block element, should this element be isolated 
			// with blank lines before and after?
			bool isolated = 
				name=="section" or name=="p" or name=="figure" or name=="table" or
				name=="style" or node.has("data-exec") or
				node.has("data-when") or node.has("data-with") or 
				node.has("data-for") or node.has("data-switch") or 
				node.has("data-include") or node.has("data-macro");
			if(isolated) blankline();

			// Is a space required for any following content
			bool space_required = false;

			// Can children trail on the element's starting line?
			bool trailing_allowed = true;

			// Start of element depends on type of element...
			if(inline_element){
				// Opening brace
				content("{");
			} else {
				// Fresh line
				newline(indent);
			}
			
			// Execute directives
			if(node.has("data-exec")){
				content(node.attr("data-exec"));
				space_required = true;

				erase_attr("data-exec");
				embedded = true;
			}
			// Execute directive output
			else if(node.has("data-out")){
				content("out");
				space_required = true;

				erase_attr("data-out");
			}
			// Style elements
			else if(name=="style"){
				std::string lang = "css";
				std::string type = node.attr("type");
				if(type=="text/css") lang = "css";
				
				content(lang);
				space_required = true;

				erase_attr("type");
				embedded = true;
			}
			// <div>s only need to be specified if 
			// 	- has no attributes
			// 	- has only flag attributes
			// 	- not a `text` or `refer` directive (which have span defaults)
			else if(name=="div"){
				unsigned int flags = 0;
				for(auto attr : attribute_list){
					if(Stencil::flag(attr)) flags++;
				}
				if(attributes==0 or flags==attributes or node.has("data-text") or node.has("data-refer")){
					content(name);
					space_required = true;
				}
			}
			// <span>s don't need to specified if a `text` or `refer` directive
			else if(name=="span"){
				if(not (node.has("data-text") or node.has("data-refer"))){
					content(name);
					space_required = true;
				}
			}
			else{
				content(name);
				space_required = true;
			}

			// Handle attributes...
			if(attributes){
				std::pair<std::string,std::string> directive;
				std::vector<std::pair<std::string,std::string>> flags;
				for(auto name : attribute_list){
					auto value = node.attr(name);
					if(Stencil::directive(name)){
						directive.first = name;
						directive.second = value;
					}
					else if(Stencil::flag(name)){
						flags.push_back({name,value});
					}
					else {
						if(space_required) content(" ");
						if(name=="id"){
							content("#"+value);
						}
						else if(name=="class"){
							// Get class attribute and split using spaces
							std::vector<std::string> classes;
							boost::split(classes,value,boost::is_any_of(" "));
							int index = 0;
							for(auto name : classes){
								if(index>0) content(" ");
								if(name.length()) content("."+name);
								index++;
							}
						}
						else {
							content("["+name+"="+value+"]");
						}
						space_required = true;
					}
				}

				// Directives
				if(directive.first.length()){
					auto name = directive.first;
					// Directive name
					if(space_required) content(" ");
					content(name.substr(5));
					// Directive argumnet
					if(not(name=="data-each" or name=="data-else" or name=="data-default")){
						auto value = directive.second;
						content(" "+value);
					}
					space_required = true;
					trailing_allowed = false;
				}

				// Flags
				if(flags.size()){
					if(space_required) content(" ");
					content("~");
					trailing_allowed = false;
			
					for(auto attr : flags){
						auto name = attr.first;
						auto value = attr.second;
						std::string flag;
						if(name=="data-hash") flag = "&"+value;
						else if(name=="data-index") flag = "^"+value;
						else if(name=="data-error" or name=="data-warning"){
							flag = (name=="data-error")?"!":"%";
							auto parts = split(value,"@");
							std::string message = parts[0];
							boost::replace_all(message,"\"","'");  // Double quote replaced with single to avoid parsing errors
							flag += "\"" + message + "\"";
							std::string location;
							if(parts.size()>1){
								flag += "@" + parts[1];
							}
						}
						else flag = name.substr(5);
						content(" "+flag);
						space_required = true;
					}
				}
			}

			// Generate children
			if(children>0){
				if(inline_element){
					// Insert a separating space
					content(" ");
					// If trailing is not allowed then need to separate with
					// space surrounded semicolon
					if(not trailing_allowed) content(": ");
					// Generate children (which should all be inline)
					generate_children(children_list);
				}
				else {
					if(not embedded) {
						// Check if this element has any block elements
						bool has_block_children = false;
						for(Node child : children_list){
							if(Html::is_block_element(child)){
								has_block_children = true;
								break;
							}
						}
						// If trailing allowed and only inline elements...
						if(trailing_allowed and not has_block_children){
							content(" ");
							generate_children(children_list);
						}
						// otherwise...
						else {
							generate_children(children_list,true,indent+"\t");
						}
					}
					else {
						// Get the code from the child nodes. Usually there will be only one, but in case there are more
						// add them all. Note that the text() method unencodes HTML special characters (e.g. &lt;) for us
						std::string code;
						for(Node child : children_list) code += child.text();
						// Trim white space (it should never be significant when at start or end)
						// Normally code will start and end with a new line (that is how it is created when parsed)
						// so remove those, and any other whitespace, for consistent Cila generation
						boost::trim(code);
						if(code.length()>0){
							// Split into lines
							std::vector<std::string> lines;
							boost::split(lines,code,boost::is_any_of("\n"));
							// Output each line, with extra indentation if it has content
							for(unsigned int index=0;index<lines.size();index++){
								auto line = lines[index];
								if(line.length()>0){
									newline(indent+"\t");
									content(line);
								} else {
									blankline();
								}
							}
						}
					}
				}
			}

			// End of element depends on type of element...
			if(inline_element){
				// Closing brace
				content("}");
			} 
			else {
				// Specifically isolate with a blankline
				if(isolated) blankline();
			}

		}
		else if(node.is_text()){
			auto text = node.text();
			
			// Escape characters used for shorthands
			boost::replace_all(text,"`","\\`");
			boost::replace_all(text,"|","\\|");
			boost::replace_all(text,"~","\\~");
			boost::replace_all(text,"@","\\@");

			// Translate HTML entities
			boost::replace_all(text,"&nbsp;"," ");
			
			content(text);
		}
		else {
			STENCILA_THROW(Exception,"Unhandled XML node type");
		}
	}

	/**
	 * Generate Cila for the children of a node
	 *
	 * @param start_as_block  Should this be started off like a block element with a newline?
	 * @param indent Indentation _for children_
	 */
	void generate_children(Nodes children, bool start_as_block=false, const std::string& indent=""){
		// If a child in a block element it must ne followed by a newline
		bool previous_was_block = start_as_block;
		for(Node child : children){
			bool child_is_block = Html::is_block_element(child);
			if(not child_is_block and previous_was_block) newline(indent);
			generate_node(child,indent);
			previous_was_block = child_is_block;
		}
	}

	/**
	 * Generate Cila from a `Html::Node`
	 */
	std::string generate(Node node){
		cila.str("");
		generate_node(node);
		return trim(cila.str());
	}

};

#if !defined(STENCILA_CILA_INLINE)

Stencil& Stencil::cila(const std::string& string){
	CilaParser().parse(*this,string);
	return *this;
}

std::string Stencil::cila(void) const {
	return CilaGenerator().generate(*this);
}

#endif

}
