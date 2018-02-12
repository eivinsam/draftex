const caret = (() =>
{
    const result = document.createElement('span');
    result.classList.add('caret');
    return result;
})();

function skipSpace(text: string, offset = 0)
{
    while (offset < text.length && text.charCodeAt(offset) <= 32)
        offset++;
    return offset;
}

function destar(name: string)
{
    return name.replace('*', '');
}

function ignoreText(out: Element, text: string)
{
    if (out.lastChild instanceof Element && out.lastChild.className == 'ignored')
        appendText(out.lastChild, text);
    else
        appendElement(out, 'span', 'ignored').textContent = text;
}

function appendText(out: Element, text: string)
{
    if (out.lastChild instanceof Text)
        out.lastChild.appendData(text);
    else
        out.appendChild(document.createTextNode(text));
}

function appendElement(out: Element, tag: string, className?: string)
{
    const result = out.appendChild(document.createElement(tag));
    if (className)
        result.className = className;
    return result;
}

function appendContent(out: Element, source: Element)
{
    for (let n = source.firstChild; n != null; n = source.firstChild)
        out.appendChild(n);
}

function isAlpha(ch: number) { return (ch > 64 && ch < 91) || (ch > 96 && ch < 123); }
function isSpace(ch: number) { return (ch >= 0 && ch <= 32); }

const SPACE = ' '.charCodeAt(0);
const COMMAND = '\\'.charCodeAt(0);
const COMMENT = '%'.charCodeAt(0);
const ARGUMENT = '#'.charCodeAt(0);
const MATHMODE = '$'.charCodeAt(0);
const TABULATE = '&'.charCodeAt(0);
const NEWLINE = '\n'.charCodeAt(0);
const CURLY_IN = '{'.charCodeAt(0);
const CURLY_OUT = '}'.charCodeAt(0);
const SQUARE_IN = '['.charCodeAt(0);
const SQUARE_OUT = ']'.charCodeAt(0);
const SUBSCRIPT = '_'.charCodeAt(0);
const SUPERSCRIPT = '^'.charCodeAt(0);


class ReadState
{
    constructor(public readonly text: string, public next: number) { }

    skip() { this.next++; }

    get hasNext() { return this.next < this.text.length; }
    get nextCode() { return this.text.charCodeAt(this.next); }
    get nextChar() { return this.text.charAt(this.next); }

    get nextIsAlpha() { return isAlpha(this.nextCode); }
    get nextIsDigit()
    {
        const ch = this.nextCode;
        return (ch >= 48 && ch < 58);
    }
    get nextIsSpace()
    {
        return isSpace(this.nextCode);
    }

    readChar() { this.next++; return this.text.charAt(this.next - 1); }
    readSquare()
    {
        if (!this.hasNext || this.nextCode != SQUARE_IN)
            return null;
        this.skip();
        const start = this.next;
        loop:
        while (this.hasNext)
            switch (this.nextCode)
            {
                case SQUARE_IN: throw new Error('unexpected [ before ]');
                case SQUARE_OUT: break loop;
                default: this.skip(); break;
            }

        if (!this.hasNext)
            throw new Error('missing ]');
        this.skip();
        return this.text.substring(start, this.next - 1);
    }
    readCurly()
    {
        if (!this.hasNext || this.nextCode != CURLY_IN)
            throw new Error('expected group');
        this.skip();
        const start = this.next;
        loop:
        while (this.hasNext)
            switch (this.nextCode)
            {
                case CURLY_IN: throw new Error('unexpected { in flat group');
                case CURLY_OUT: break loop;
                default: this.skip(); break;
            }

        if (!this.hasNext)
            throw new Error('missing group end');
        this.skip();
        return this.text.substring(start, this.next-1);
    }
    readCommand()
    {
        if (!this.hasNext || this.nextCode != COMMAND)
            throw new Error('expected \\');
        this.skip();
        if (this.nextIsAlpha)
        {
            const start = this.next;
            this.skip();
            while (this.hasNext && this.nextIsAlpha)
                this.skip();
            return this.text.substring(start, this.next);
        }

        return this.readChar();
    }
    readComment()
    {
        if (!this.hasNext || this.nextCode != COMMENT)
            throw new Error('expected comment');
        this.skip();
        const start = this.next;
        while (this.nextCode != NEWLINE)
            this.skip();
        this.skip();
        return this.text.substring(start, this.next);
    }
}

//class State
//{
//    constructor(public readonly src: ReadState, public readonly name: string, public readonly mode: Mode) { }

//    pushName(new_name: string, new_mode?: Mode)
//    {
//        return new State(this.src, new_name, new_mode ? new_mode : this.mode);
//    }
//}

//type CommandParser = (State, Element) => void;


//const commands: { [index: string]: CommandParser } =
//    {
//        'begin': parseBegin,
//        'title': styledArg,
//        'author': styledArg,
//        'section': styledDeparArg,
//        'subsection': styledDeparArg,
//        'subsubsection': styledDeparArg,
//        'caption': styledArg,
//        'cite': styledArg,
//        'emph': styledArg,
//        'ref': parseRef,
//        'autoref': parseRef,
//        'label': parseLabel,
//        'overline': styledArg,
//        'mbox': styledArg,
//        'text': styledArg,
//        'textrm': styledArg,
//        'textbf': styledArg,
//        'mathbf': styledArg,
//        'boldsymbol': styledArg, 
//        'operatorname': styledArg,
//        'mathcal': parseMathcal,
//        'mathbb': parseMathbb,
//        'frac': parseFraction
//    };

//const symbols =
//    {
//        'alpha': 'ɑ',
//        'delta': 'δ',
//        'Delta': 'Δ',
//        'phi': 'ϕ',
//        'sigma': 'σ',
//        'Sigma': 'Σ',
//        'sum': 'Σ',
//        'theta': 'θ', 
//        'Theta': 'Θ',
//        'times': '×',
//        'in': '∈',
//        'leq': '&le;',
//        'prec': "\u227A",
//        'succ': "\u227B",
//        'lbrace': '{',
//        'rbrace': '}',
//        ' ': '␣',
//        ',': '␣',
//        'rightarrow': '→',
//        'sim': '~'
//    }


//function fixedFromCharCode(codePt)
//{
//    if (codePt > 0xFFFF)
//    {
//        codePt -= 0x10000;
//        return String.fromCharCode(0xD800 + (codePt >> 10), 0xDC00 + (codePt & 0x3FF));
//    }
//    else
//    {
//        return String.fromCharCode(codePt);
//    }
//}

//const mathbb_lookup =
//    {
//        'C': '\u2102',
//        'H': '\u210d',
//        'N': '\u2115',
//        'P': '\u2119',
//        'Q': '\u211a',
//        'R': '\u211d',
//        'Z': '\u2124'
//    }

//function parseMathbb(s: State, out: HTMLElement)
//{
//    const src = s.src.readCurly();
//    const dst = out.appendChild(document.createElement('span'));
//    dst.className = 'mathbb';

//    for (let i = 0; i < src.length; i++)
//    {
//        const lu = mathbb_lookup[src.charAt(i)];
//        if (lu !== undefined)
//            appendText(dst, lu);
//        else
//            appendText(dst, fixedFromCharCode(src.charCodeAt(i) + (0x1d538 - 0x41)));
//    }
//}

//const mathcal_lookup =
//    {
//        'B': '\u212c',
//        'E': '\u2130',
//        'F': '\u2131',
//        'H': '\u210b',
//        'I': '\u2110',
//        'L': '\u2112',
//        'K': '\u2133',
//        'R': '\u211b',
//        'e': '\u212f',
//        'g': '\u210a',
//        'o': '\u2134'
//    }
//function parseMathcal(s: State, out: Element)
//{
//    const src = s.src.readCurly();
//    const dst = appendElement(out, 'span', 'mathcal');

//    for (let i = 0; i < src.length; i++)
//    {
//        const lu = mathcal_lookup[src.charAt(i)];
//        if (lu !== undefined)
//            appendText(dst, lu);
//        else
//            appendText(dst, fixedFromCharCode(src.charCodeAt(i) + (0x1d49c - 0x41)));
//    }
//}

//function parseFraction(s: State, out: Element)
//{
//    out = appendElement(out, 'div', 'frac');

//    parseArgument(s, out);
//    out.lastElementChild.className = 'numerator';

//    out.appendChild(document.createElement('hr'));

//    parseArgument(s, out);
//    out.lastElementChild.className = 'denominator';
//}

//function parseComment(s: State, out: Element)
//{
//    if (!s.src.hasNext || s.src.nextCode != COMMENT)
//        throw new Error('expected comment');
//    s.src.skip();
//    let start = s.src.next;
//    while (s.src.nextCode != NEWLINE)
//        s.src.skip();
//    s.src.skip();
//    appendElement(out, 'span', 'comment').textContent = s.src.text.substring(start, s.src.next);
//}

//function createSpan(classname: string, content: Node)
//{
//    const result = document.createElement('span');
//    result.classList.add(classname);
//    result.appendChild(content);
//    return result;
//}
//function createTextSpan(classname: string, content: string)
//{
//    return createSpan(classname, document.createTextNode(content));
//}

//function ignoreSpace(s: State, out: Element)
//{
//    if (s.src.hasNext && s.src.nextIsSpace)
//    {
//        out = appendElement(out, 'span', 'ignored');
//        while (s.src.hasNext && s.src.nextIsSpace)
//            appendText(out, s.src.readChar());
//    }
//}

//function parseArgument(s: State, out: Element)
//{
//    ignoreSpace(s, out);

//    while (s.src.hasNext)
//    {
//        switch (s.src.nextCode)
//        {
//            case COMMENT: parseComment(s, out); ignoreSpace(s, out); continue;
//            case COMMAND:
//                out = appendElement(out, 'span', 'curly');
//                out = appendElement(out, 'span', 'command');
//                out.textContent = s.src.readCommand();
//                return;
//            case CURLY_IN: parseCurlyNoexpand(s, out); return;
//            default:
//                out = appendElement(out, 'span', 'curly');
//                out.textContent = s.src.readChar();
//                return;
//        }
//    }
//    throw new Error('unexpected end of input');
//}

//function parseCurly(s: State, out: Element)
//{
//    if (!s.src.hasNext || s.src.nextCode != CURLY_IN)
//        throw new Error('expected group');
//    s.src.skip();
//    parseEnv(s.pushName('curly'), 'span', out);
//}

//function parseCurlyNoexpand(s: State, out: Element)
//{
//    if (!s.src.hasNext || s.src.nextCode != CURLY_IN)
//        throw new Error('expected group');
//    s.src.skip();

//    out = appendElement(out, 'span', 'curly');

//    while (s.src.hasNext)
//    {
//        switch (s.src.nextCode)
//        {
//            case CURLY_OUT: s.src.skip(); return;
//            case COMMENT: parseComment(s, out); ignoreSpace(s, out); continue;
//            case CURLY_IN: parseCurlyNoexpand(s, out); continue;
//            case COMMAND: appendElement(out, 'span', 'command').textContent = s.src.readCommand(); continue;
//            case ARGUMENT:
//                s.src.skip();
//                if (!s.src.nextIsDigit)
//                    throw new Error('expected parameter number');
//                appendElement(out, 'span', 'argument').textContent = s.src.readChar();
//                continue;
//            default:
//                appendText(out, s.src.readChar());
//                continue;
//        }
//    }

//}

//function parseSubsup(s: State, out: Element)
//{
//    out = appendElement(out, s.src.nextCode == SUPERSCRIPT ? 'sup' : 'sub');
//    s.src.skip();
//    if (!s.src.hasNext)
//        throw new Error('expected more characters after subscript');
//    switch (s.src.nextCode)
//    {
//        case CURLY_IN:
//            parseCurly(s, out);
//            appendContent(out, out.firstElementChild);
//            out.firstElementChild.remove();
//            break;
//        case COMMAND:
//            parseCommand(s.pushName(s.src.readCommand()), out);
//            break;
//        default:
//            out.textContent = s.src.readChar();
//            break;
//    }
//}


//function styledArg(s: State, out: Element)
//{
//    parseArgument(s, out);
//    out.lastElementChild.className = s.name;


//    parseCurly(s, out);
//}
//function styledDeparArg(s: State, out: Element)
//{
//    parseCurly(s, out);
//    out.lastElementChild.className = s.name;
//    if (out instanceof HTMLParagraphElement)
//    {
//        const result = out.lastChild;
//        const parent = out.parentNode;
//        parent.insertBefore(result, out);

//        if (out.firstChild != null)
//        {
//            const split_par = document.createElement('p');
//            while (out.firstChild != null)
//                split_par.appendChild(out.firstChild);
//            parent.insertBefore(split_par, result);
//        }
//    }
//}

//function parseLabel(s: State, out: Element)
//{
//    const id = s.src.readCurly();
//    const span = appendElement(out, 'span', 'label');
//    span.id = id;
//    span.textContent = id;
//}

//function parseRef(s: State, out: Element)
//{
//    const target = s.src.readCurly();
//    const anchor = out.appendChild(document.createElement('a'));
//    anchor.href = '#' + target;
//    anchor.textContent = target;
//}

//class ElementSource
//{
//    private n: Node;
//    constructor(e: Element) { this.n = e.firstChild; }

//    get next() { return this.n; }

//    pop()
//    {
//        const result = this.n;
//        if (this.n != null)
//            this.n = this.n.nextSibling;
//        return result;
//    }
//}

//function substitute(args: Node, out: Node)
//{
//    for (let n = out.firstChild; n != null; n = n.nextSibling)
//    {
//        if (n instanceof Element)
//        {
//            if (n.className == 'argument')
//            {
//                const argn = Number(n.textContent);
//                const arg = args.childNodes.item(argn - 1);
//                if (argn === null)
//                    throw new Error('invalid argument number: ' + argn);

//                for (let rep = arg.firstChild; rep != null; rep = rep.nextSibling)
//                    out.insertBefore(rep.cloneNode(true), n);
//                out.removeChild(n);
//            }
//            else
//                substitute(args, n);
//        }
//    }
//}

//commands.newcommand = (s: State, out: Element) =>
//{
//    out = appendElement(out, 'div');
//    out.appendChild(createTextSpan('command', s.name));

//    parseCurlyNoexpand(s, out);
//    const cmd = out.lastElementChild.firstElementChild;
//    if (cmd.className != 'command')
//        throw new Error('expected command name as first argument to newcommand');
//    if (cmd.nextSibling != null)
//        throw new Error('first argument to newcommand contains more than a command symbol');

//    const argc = s.src.readSquare();
//    if (argc !== null)
//    {
//        out.appendChild(createTextSpan('square', argc));
//    }

//    parseCurlyNoexpand(s, out);
//    const definition = out.lastChild;

//    commands[cmd.textContent] = (s: State, out: Element) =>
//    {
//        const result = out.appendChild(definition.cloneNode(true) as HTMLElement);
//        result.className = 'expanded-' + cmd.textContent;


//        if (argc != null)
//        {
//            const temp = document.createElement('div');
//            for (let i = 0; i < +argc; i++)
//            {
//                const start = s.src.next;
//                parseArgument(s, temp);
//                result.dataset['arg' + i] = s.src.text.substring(start, s.src.next);
//            }
//            substitute(temp, result);
//        }
//    };
//}


//function parseBegin(s: State, out: Element)
//{
//    const name = s.src.readCurly();
//    let mode = Mode.text;
//    switch (destar(name))
//    {
//        case 'math':
//        case 'displaymath':
//        case 'cases':
//        case 'align':
//            mode = Mode.math;
//    }
//    parseEnv(s.pushName(name, mode), 'div', out);
//}

//const enum Env { par, table };
//const enum Mode { text, math };

//const environ: {[index: string]: Env} =
//    {
//        'align': Env.table,
//        'cases': Env.table,
//        'document': Env.par,
//        'tabular': Env.table,
//        'root': Env.par
//    };

//function allSpace(el: Node)
//{
//    if (el instanceof Text)
//    {
//        for (let i = 0; i < el.data.length; i++)
//            if (el.data.charCodeAt(i) > SPACE)
//                return false;
//        return true;
//    }
//    for (let n = el.firstChild; n != null; n = n.nextSibling)
//        if (!allSpace(n))
//            return false;
//    return true;
//}

//function parseEnv(s: State, tag: string, out: Element)
//{
//    const nostar_name = destar(s.name);
//    const env = environ[nostar_name];
//    const first_column_align = nostar_name == 'align' ? 'right' : 'left';
//    if (nostar_name == 'itemize') tag = 'ul';
//    switch (env)
//    {
//        case Env.table:
//            out = appendElement(out, 'table', nostar_name);
//            out = appendElement(out, 'tr');
//            out = appendElement(out, 'td', first_column_align);
//            break;
//        case Env.par:
//            out = appendElement(out, 'div', nostar_name);
//            out = appendElement(out, 'p');
//            break;
//        default:
//            out = appendElement(out, tag, nostar_name);
//            break;
//    }

//    let prev_newline = false;
//    while (s.src.hasNext)
//    {
//        const ch = s.src.nextCode;
//        switch (ch)
//        {
//            case COMMENT: parseComment(s, out); break;
//            case TABULATE:
//                if (env != Env.table)
//                    throw new Error('unexpected tabulation');
//                s.src.skip();
//                out = appendElement(out.parentElement, 'td', 'left');
//                break;
//            case MATHMODE:
//                s.src.skip();
//                if (s.name == 'short-math')
//                    return;
//                parseEnv(s.pushName('short-math', Mode.math), 'span', out);
//                break;
//            case SUBSCRIPT:
//            case SUPERSCRIPT:
//                if (s.mode != Mode.math)
//                    throw new Error('unexpected sub/superscript outside of math mode');
//                parseSubsup(s, out);
//                break;
//            case COMMAND:
//                const cmd = s.src.readCommand();
//                switch (cmd)
//                {
//                    case '\\':
//                        switch (env)
//                        {
//                            case Env.table:
//                                out = appendElement(out.parentElement.parentElement, 'tr');
//                                out = appendElement(out, 'td', first_column_align);
//                                break;
//                            case Env.par:
//                                appendElement(out, 'br');
//                                break;
//                        }
//                        break;
//                    case '[': parseEnv(s.pushName('short-displaymath', Mode.math), 'div', out); break;
//                    case ']': 
//                        if (s.name != 'short-displaymath')
//                            throw new Error('unexpected \\]');
//                        return;
//                    case 'end':
//                        if (s.name == 'item')
//                        {
//                            s.src.next -= cmd.length+1;
//                            return;
//                        }
//                        const endof = s.src.readCurly();
//                        if (s.name != endof)
//                            throw new Error('begin/end mismatch: ' + name + '/' + endof);
//                        return;
//                    case 'left':
//                        if (s.mode != Mode.math)
//                            throw new Error('illegal context for \\left');
//                        parseEnv(s.pushName('mathspan'), 'span', out);
//                        break;
//                    case 'right':
//                        if (s.name != 'mathspan')
//                            throw new Error('illegal context for \\right');
//                        return;
//                    case 'item':
//                        switch (nostar_name)
//                        {
//                            case 'itemize': parseEnv(s.pushName('item'), 'li', out); break;
//                            case 'item': s.src.next -= cmd.length + 1; return;
//                            default: console.log('item outside itemize, ignoring'); break;
//                        }
//                        break;
//                    default:
//                        parseCommand(s.pushName(cmd), out);
//                        break;
//                }
//                break;
//            case CURLY_IN: parseCurly(s, out); break;
//            case CURLY_OUT:
//                if (s.name != 'curly')
//                    throw new Error('unexpected end of group');
//                s.src.skip();
//                return;
//            case NEWLINE:
//                if (!prev_newline)
//                    prev_newline = true;
//                else if (prev_newline && env == Env.par && !allSpace(out))
//                {
//                    // make paragraph break
//                    out = appendElement(out.parentElement, 'p');
//                    prev_newline = false;
//                }
//                // fallthrough!
//            default:
//                appendText(out, s.src.readChar());
//                if (ch <= SPACE)
//                    continue;
//                break;
//        }
//        prev_newline = false;
//    }
//}


//function parseCommand(s: State, out: Element)
//{
//    const proc = commands[s.name];
//    if (proc !== undefined)
//        return proc(s, out);

//    const sym = symbols[s.name];
//    if (sym !== undefined)
//    {
//        appendText(out, sym);
//        if (s.src.nextCode == SPACE)
//            s.src.skip();
//        return;
//    }

//    out = appendElement(out, 'span', 'command');
//    out.textContent = s.name;
//}


const enum Parsing { newline, space, text };

function tokenize(src: ReadState, out: Element)
{
    let state = Parsing.newline;
    while (src.hasNext)
    {
        switch (src.nextCode)
        {
            case COMMENT:
                appendElement(out, 'span', 'comment')
                    .textContent = src.readComment();
                state = Parsing.newline;
                continue;
            case COMMAND:
                const first = (appendElement(out, 'span', 'command')
                    .textContent = src.readCommand()).charCodeAt(0);
                if (first == SPACE || isAlpha(first))
                    state = Parsing.space;
                else
                    state = Parsing.text;
                continue;
            case NEWLINE:
                src.skip();
                switch (state)
                {
                    case Parsing.newline: appendElement(out, 'span', 'command').textContent = 'par'; break;
                    case Parsing.text: appendText(out, '\n'); break;
                    default: ignoreText(out, '\n'); break;
                }
                state = Parsing.newline;
                continue;
            case ARGUMENT:
                src.skip();
                appendElement(out, 'spam', 'argument').textContent = src.readChar();
                state = Parsing.text;
                continue;
            case CURLY_IN:
                src.skip();
                tokenize(src, appendElement(out, 'span', 'curly'));
                continue;
            case CURLY_OUT:
                src.skip();
                if (out.className != 'curly')
                    throw new Error('unmatched }');
                return;
            default:
                if (isSpace(src.nextCode))
                {
                    if (state != Parsing.text)
                        ignoreText(out, src.readChar());
                    else
                    {
                        appendText(out, src.readChar());
                        state = Parsing.space;
                    }
                }
                else
                {
                    appendText(out, src.readChar());
                    state = Parsing.text;
                }
                continue;
        }
    }
}

function insertElement(next: Node, tag: string, className?: string, textContent?: string)
{
    const new_element = document.createElement(tag);
    if (className)
    {
        new_element.className = className;
        if (textContent)
            new_element.textContent = textContent;
    }
    next = next.nextSibling;
    return next.parentElement.insertBefore(new_element, next);
}

class Result
{
    constructor(public readonly next: Node, public readonly end: string = null) { }
}

class Argument
{
    public readonly tokens: Element;
    constructor(from: Element)
    {
        this.tokens = null;
        // TODO: save text
        while (from.firstChild instanceof Element && from.firstChild.className == 'ignored')
        {
            from.removeChild(from.firstChild);
        }

        if (from.firstChild instanceof Element)
        {
            if (from.firstChild.className == 'curly')
                this.tokens = from.removeChild(from.firstChild);
            else if (from.firstChild.className == 'command')
            {
                this.tokens = document.createElement('span');
                this.tokens.className = 'curly';
                this.tokens.appendChild(from.removeChild(from.firstChild));
            }
        }
        else if (from.firstChild instanceof Text)
        {
            if (from.firstChild.data.length > 1)
                from.firstChild.splitText(1);
            this.tokens = document.createElement('span');
            this.tokens.className = 'curly';
            this.tokens.appendChild(from.removeChild(from.firstChild));
        }
    }
}


class Commands
{
    _default(to: Element, from: Element) { to.appendChild(from.firstChild); return null; }
    begin(to: Element, from: Element)
    {
        from.removeChild(from.firstChild);
        const spec = from.firstChild;
        if (spec instanceof Element && spec.className == 'curly')
        {
            from.removeChild(from.firstChild);
            expand(to, from, spec.textContent);
        }
        else
        {
            appendElement(to, 'span', 'error').textContent = 'begin: expected environment name';
        }
        return null;
    }
    end(to: Element, from: Element)
    {
        from.removeChild(from.firstChild);
        const spec = from.firstChild;
        if (spec instanceof Element && spec.className == 'curly')
        {
            from.removeChild(from.firstChild);
            return spec.textContent;
        }
        else
        {
            appendElement(to, 'span', 'error').textContent = 'end: expected environment name';
            return null;
        }
    }

    newcommand(to: Element, from: Element)
    {
        from.removeChild(from.firstChild);
        const name = new Argument(from);
        if (!name.tokens)
        {
            appendElement(to, 'span', 'error').textContent = 'newcommand: expected arguments';
            return null;
        }
        if (from.firstChild instanceof Text && from.firstChild.data.charCodeAt(0) == SQUARE_IN)
        {
            // TODO: read args!
            from.removeChild(from.firstChild);
        }
        const def = new Argument(from);
        if (!def.tokens)
        {
            appendElement(to, 'span', 'error').textContent = 'newcommand: expected '
            return null;
        }

        
    }

    frac(to: Element, from: Element)
    {
        from.removeChild(from.firstChild);
        const p = new Argument(from);
        if (p.tokens)
        {
            const q = new Argument(from);
            if (q.tokens)
            {
                to = appendElement(to, 'div', 'frac');
                to.appendChild(p.tokens); p.tokens.className = 'numerator';
                appendElement(to, 'hr');
                to.appendChild(q.tokens); q.tokens.className = 'denominator';
            }
            else
                appendElement(to, 'span', 'error').textContent = 'missing second argument to frac';
        }
        else
            appendElement(to, 'span', 'error').textContent = 'missing arguments to frac';
        
    }
}

const commands = new Commands;

class Expander
{
    _default(to: Element, from: Element): string { to.appendChild(from.firstChild); return null; }
    command(to: Element, from: Node): string { return (commands[from.firstChild.textContent] || commands._default)(to, from); }
}

const expander = new Expander;

function expand(to: Element, from: Element, name: string)
{
    to = appendElement(to, 'div', name);
    while (from.firstChild != null)
    {
        if (from.firstChild instanceof Element)
        {
            let end = (expander[from.firstChild.className] || expander._default)(to, from);
            if (end)
            {
                if (end == name)
                    return null;
                else
                    appendElement(to, 'span', 'error').textContent = 'begin/end mismatch: expected ' + name;
            }
        }
        else
            to.appendChild(from.firstChild);
    }
    return null;
}

function parseDocument(text: string)
{
    const result = appendElement(document.body, 'div');
    const tokens = appendElement(document.body, 'div', 'tokens');
    tokenize(new ReadState(text, 0), tokens);
    expand(result, tokens, 'root');
    //parseEnv(new State(new ReadState(text, 0), 'root', Mode.text), 'div', document.body);
    //let env = new Env('root', text, 0);
    //document.body.appendChild(env.element);
}

function parseUri(uri: string)
{
    fetch(uri, { cache: 'no-store' })
        .then((response) => response.text())
        .then(parseDocument);
}

function parseLocal(event)
{
    const reader = new FileReader();
    reader.onload = () => parseDocument(reader.result);
    reader.readAsText(event.target.files[0]);
}

function stepright()
{
    if (caret.nextSibling != null)
    {
        if (caret.nextSibling.nodeType == Node.TEXT_NODE)
        {
            const spacec = skipSpace(caret.nextSibling.textContent);
            const split_pos = caret.nextSibling.textContent.length - (spacec == 0 ? 1 : spacec);
            if (split_pos > 0)
            {
                caret.parentElement.insertBefore((caret.nextSibling as Text).splitText(split_pos), caret);
                caret.parentElement.normalize();
            }
            else
            {
                caret.parentElement.insertBefore(caret.nextSibling, caret);
            }
        }
        console.log('prev:', caret.previousSibling);
        console.log('next:', caret.nextSibling);
    }
    else if (caret.parentNode != null)
    {
        console.log('parent:', caret.parentNode);
    }
}

function keydown(event)
{
    console.log(event);
    switch (event.code)
    {
        case "ArrowRight": stepright(); return;
    }
}

function main()
{
    const selector = document.createElement('input');
    selector.type = 'file';

    document.body.appendChild(selector);

    selector.onchange = parseLocal;

    document.body.onkeydown = keydown;

    const args = window.location.search.replace('?', '').split('&');
    for (let i = 0; i < args.length; i++)
    {
        const arg = args[i].split('=');
        if (arg.length == 2 && arg[0] == 'src')
            parseUri(arg[1]);
    }

}
