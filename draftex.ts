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

function appendError(out: Element, message: string)
{
    appendElement(out, 'span', 'error').textContent = message;
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


function substitute(args: Element[], into: Element)
{
    for (let n = into.firstChild; n != null; n = n.nextSibling)
    {
        if (n instanceof Element)
        {
            if (n.className == 'argument')
            {
                const argi = Number(n.textContent)-1;
                if (argi < 0 || argi >= args.length)
                {
                    n.className = 'error';
                    n.textContent = '#' + n.textContent;
                }
                else
                {
                    const arg = args[argi].cloneNode(true);
                    while (arg.firstChild)
                        into.insertBefore(arg.firstChild, n);
                    n = n.previousSibling;
                    into.removeChild(n.nextSibling);
                }
            }
            else
                substitute(args, n);
        }
    }
}


class Commands
{
    _default(to: Element, from: Element) { to.appendChild(from.firstChild); return null; }
    _styled_nullary(to: Element, from: Element, tag: string)
    {
        appendElement(to, tag, from.firstChild.textContent);
        from.removeChild(from.firstChild);
        return null;
    }
    _styled_unary(to: Element, from: Element)
    {
        const name = from.firstChild.textContent;
        from.removeChild(from.firstChild);
        const arg = new Argument(from);
        if (!arg.tokens)
        {
            appendError(to, name + ': expected single argument');
            return null;
        }
        arg.tokens.className = name;
        to.appendChild(arg.tokens);
        return null;
    }
    begin(to: Element, from: Element)
    {
        from.removeChild(from.firstChild);
        const spec = from.firstChild;
        if (spec instanceof Element && spec.className == 'curly')
        {
            from.removeChild(from.firstChild);
            to = appendElement(to, 'div', spec.textContent);
            if (spec.textContent == 'document')
            {
                (to as HTMLElement).contentEditable = 'true';
            }
            to = appendElement(to, 'p');
            expand(to, from, spec.textContent);
        }
        else
        {
            appendError(to, 'begin: expected environment name');
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
            appendError(to, 'end: expected environment name');
            return null;
        }
    }
    par(to: Element, from: Element)
    {
        from.removeChild(from.firstChild);
        return 'par';
    }

    newcommand(to: Element, from: Element)
    {
        from.removeChild(from.firstChild);
        const name_arg = new Argument(from);
        if (!name_arg.tokens)
        {
            appendError(to, 'newcommand: expected name');
            return null;
        }
        let argc = 0;
        if (from.firstChild instanceof Text && from.firstChild.data.charCodeAt(0) == SQUARE_IN)
        {
            const end = from.firstChild.data.indexOf(']');
            if (end < 0)
            {
                appendError(to, 'newcommand: unmatched [: ');
                to.appendChild(from.firstChild);
                return null;
            }
            if (from.firstChild.data.length > end + 1)
                from.firstChild.splitText(end + 1);
            argc = Number(from.firstChild.data.substring(1, end));
            from.removeChild(from.firstChild);
        }
        const def_arg = new Argument(from);
        if (!def_arg.tokens)
        {
            appendError(to, 'newcommand: expected definition');
            return null;
        }
        if (!(name_arg.tokens.childNodes.length == 1 &&
            name_arg.tokens.firstChild instanceof Element &&
            name_arg.tokens.firstChild.className == 'command'))
        {
            appendError(to, 'newcommand: first argument should be a single command token');
            return null;
        }
        const name = name_arg.tokens.firstChild.textContent;
        this[name] = (to: Element, from: Element) =>
        {
            from.removeChild(from.firstChild);
            const body = def_arg.tokens.cloneNode(true) as Element;
            if (argc > 0)
            {
                const args: Element[] = [];
                for (let i = 0; i < argc; i++)
                {
                    const arg = new Argument(from);
                    if (!arg.tokens)
                    {
                        appendError(to, name + ': too few arguments');
                        return null;
                    }
                    args.push(arg.tokens);
                }
                substitute(args, body);
            }
            expand(to, body, null);
        }

        to = appendElement(to, 'div', 'newcommand');
        appendElement(to, 'span', 'command').textContent = 'newcommand';
        to.appendChild(name_arg.tokens);
        if (argc > 0)
            appendElement(to, 'span', 'square').textContent = argc.toString();
        to.appendChild(def_arg.tokens);
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
                appendElement(to, 'div', 'numerator');
                expand(to.lastElementChild, p.tokens, null);
                appendElement(to, 'div', 'denominator');
                expand(to.lastElementChild, q.tokens, null);
            }
            else
                appendError(to, 'missing second argument to frac');
        }
        else
            appendError(to, 'missing arguments to frac');
        
    }

    title(to: Element, from: Element) { return this._styled_unary(to, from); }
    maketitle(to: Element, from: Element) { return this._styled_nullary(to, from, 'hr'); }
}

const commands = new Commands;

class Expander
{
    _default(to: Element, from: Element): string { to.appendChild(from.firstChild); return null; }
    command(to: Element, from: Node): string
    {
        return (commands[from.firstChild.textContent] || commands._default).call(commands, to, from);
    }
}

const expander = new Expander;

function expand(to: Element, from: Element, name: string)
{
    while (from.firstChild != null)
    {
        if (from.firstChild instanceof Element)
        {
            let end = (expander[from.firstChild.className] || expander._default)(to, from);
            if (end)
            {
                if (end == name)
                    return null;
                else if (end == 'par')
                {
                    if (name == 'document')
                    {
                        to = appendElement(to.parentElement, 'p');
                    }
                    if (name == 'root')
                        appendElement(to, 'div').innerHTML = '&nbsp;';
                }
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
    const result = document.body.children[1];
    result.innerHTML = '';
    const tokens = document.createElement('div');
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

function keydown(event: KeyboardEvent)
{
    console.log(event);
    switch (event.code)
    {
        case "Enter": return false;
        default:
            return true;
    }
}

function main()
{
    const selector = document.body.children[0];
    const statusbar = document.body.children[2];
    if (selector instanceof HTMLInputElement)
        selector.onchange = parseLocal;

    document.body.onkeydown = keydown;

    const args = window.location.search.replace('?', '').split('&');
    let got_source = false;
    for (let i = 0; i < args.length; i++)
    {
        const arg = args[i].split('=');
        if (arg.length == 2 && arg[0] == 'src')
        {
            parseUri(arg[1]);
            got_source = true;
        }
    }
    if (!got_source)
    {
        parseDocument("\\documentclass{article}\\begin{document}\\title{New Document}\\maketitle\\end{document}");
    }

    document.onselectionchange = () =>
    {
        const sel = document.getSelection();

        console.log(sel.focusNode);



        let parent = sel.focusNode;
        if (parent instanceof Text)
            parent = parent.parentElement;

        statusbar.textContent = '';
        for (let n = parent as Element; n && !n.classList.contains('root'); n = n.parentElement)
        {
            const item = (n.classList.item(0) == 'context' ? n.classList.item(1) : n.classList.item(0));
            if (item)
                statusbar.textContent = item + ' ' + statusbar.textContent;
        }
            

        const old_context = document.getElementsByClassName('context');
        for (let i = 0; i < old_context.length; i++)
            if (old_context[i] != parent)
                (old_context[i] as Element).classList.remove('context');
        if (parent instanceof Element && !parent.classList.contains('context'))
            parent.classList.add('context');
    };
}
