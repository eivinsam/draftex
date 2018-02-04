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

function appendText(text: string, out: HTMLElement)
{
    if (out.lastChild == null || out.lastChild.nodeType != Node.TEXT_NODE)
        out.appendChild(document.createTextNode(text));
    else
        (out.lastChild as Text).appendData(text);
}

class ReadState
{
    constructor(public readonly text: string, public next: number) { }

    skip() { this.next++; }

    get hasNext() { return this.next < this.text.length; }
    get nextCode() { return this.text.charCodeAt(this.next); }
    get nextChar() { return this.text.charAt(this.next); }

    get nextIsAlpha()
    {
        const ch = this.nextCode;
        return (ch > 64 && ch < 91) || (ch > 96 && ch < 123);
    }
    get nextIsDigit()
    {
        const ch = this.nextCode;
        return (ch >= 48 && ch < 58);
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
}

class State
{
    constructor(public readonly src: ReadState, public readonly name: string,
        public readonly mode: Mode, public readonly out: HTMLElement) { }

    pushOutput(new_out: HTMLElement) { return new State(this.src, this.name, this.mode, new_out); }
    pushName(new_name: string, new_mode?: Mode)
    {
        return new State(this.src, new_name, new_mode ? new_mode : this.mode, this.out);
    }

    appendElement(tag: string, className?: string)
    {
        const result = this.out.appendChild(document.createElement(tag));
        if (className)
            result.className = className;
        return result;
    }
}

const commands: { [index: string]: (State) => void } =
    {
        'begin': parseBegin,
        'newcommand': parseNewcommand,
        'title': styledArg,
        'author': styledArg,
        'section': styledDeparArg,
        'subsection': styledDeparArg,
        'subsubsection': styledDeparArg,
        'caption': styledArg,
        'cite': styledArg,
        'emph': styledArg,
        'ref': parseRef,
        'autoref': parseRef,
        'label': parseLabel,
        'overline': styledArg,
        'mbox': styledArg,
        'text': styledArg,
        'textrm': styledArg,
        'textbf': styledArg,
        'mathbf': styledArg,
        'boldsymbol': styledArg, 
        'operatorname': styledArg,
        'mathcal': parseMathcal,
        'mathbb': parseMathbb,
        'frac': parseFraction
    };

const symbols =
    {
        'alpha': 'ɑ',
        'delta': 'δ',
        'Delta': 'Δ',
        'phi': 'ϕ',
        'sigma': 'σ',
        'Sigma': 'Σ',
        'sum': 'Σ',
        'theta': 'θ', 
        'Theta': 'Θ',
        'times': '×',
        'in': '∈',
        'leq': '&le;',
        'prec': "\u227A",
        'succ': "\u227B",
        'lbrace': '{',
        'rbrace': '}',
        ' ': '␣',
        ',': '␣',
        'rightarrow': '→',
        'sim': '~'
    }


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

function fixedFromCharCode(codePt)
{
    if (codePt > 0xFFFF)
    {
        codePt -= 0x10000;
        return String.fromCharCode(0xD800 + (codePt >> 10), 0xDC00 + (codePt & 0x3FF));
    }
    else
    {
        return String.fromCharCode(codePt);
    }
}

const mathbb_lookup =
    {
        'C': '\u2102',
        'H': '\u210d',
        'N': '\u2115',
        'P': '\u2119',
        'Q': '\u211a',
        'R': '\u211d',
        'Z': '\u2124'
    }

function parseMathbb(s: State)
{
    const src = s.src.readCurly();
    const dst = s.out.appendChild(document.createElement('span'));
    dst.className = 'mathbb';

    for (let i = 0; i < src.length; i++)
    {
        const lu = mathbb_lookup[src.charAt(i)];
        if (lu !== undefined)
            appendText(lu, dst);
        else
            appendText(fixedFromCharCode(src.charCodeAt(i) + (0x1d538 - 0x41)), dst);
    }
}

const mathcal_lookup =
    {
        'B': '\u212c',
        'E': '\u2130',
        'F': '\u2131',
        'H': '\u210b',
        'I': '\u2110',
        'L': '\u2112',
        'K': '\u2133',
        'R': '\u211b',
        'e': '\u212f',
        'g': '\u210a',
        'o': '\u2134'
    }
function parseMathcal(s: State)
{
    const src = s.src.readCurly();
    const dst = s.out.appendChild(document.createElement('span'));
    dst.className = 'mathcal';

    for (let i = 0; i < src.length; i++)
    {
        const lu = mathcal_lookup[src.charAt(i)];
        if (lu !== undefined)
            appendText(lu, dst);
        else
            appendText(fixedFromCharCode(src.charCodeAt(i) + (0x1d49c - 0x41)), dst);
    }
}

function parseFraction(s: State)
{
    const table = s.out.appendChild(document.createElement('div'));
    table.className = 'frac';
    parseCurly(s);
    table.appendChild(document.createElement('hr'));
    parseCurly(s);
    table.firstElementChild.className = 'numerator';
    table.lastElementChild.className = 'denominator';
}

function parseComment(s: State)
{
    if (!s.src.hasNext || s.src.nextCode != COMMENT)
        throw new Error('expected comment');
    s.src.skip();
    let start = s.src.next;
    while (s.src.nextCode != NEWLINE)
        s.src.skip();
    const comment = s.appendElement('span', 'comment');
    comment.textContent = s.src.text.substring(start, s.src.next);
}

function createSpan(classname: string, content: Node)
{
    const result = document.createElement('span');
    result.classList.add(classname);
    result.appendChild(content);
    return result;
}
function createTextSpan(classname: string, content: string)
{
    return createSpan(classname, document.createTextNode(content));
}


function parseCurly(s: State)
{
    if (!s.src.hasNext || s.src.nextCode != CURLY_IN)
        throw new Error('expected group');
    s.src.skip();
    parseEnv(s.pushName('curly'), 'span');
}

function parseSubsup(s: State)
{
    s = s.pushOutput(s.appendElement(s.src.nextCode == SUPERSCRIPT ? 'sup' : 'sub'));
    s.src.skip();
    if (!s.src.hasNext)
        throw new Error('expected more characters after subscript');
    switch (s.src.nextCode)
    {
        case CURLY_IN:
            parseCurly(s);
            // unpack braces
            for (let n = s.out.firstElementChild.firstChild; n != null; n = n.nextSibling)
                s.out.appendChild(n);
            s.out.firstElementChild.remove();
            break;
        case COMMAND:
            parseCommand(s.pushName(s.src.readCommand()));
            break;
        default:
            s.out.textContent = s.src.readChar();
            break;
    }
}


function styledArg(s: State)
{
    parseCurly(s);
    s.out.lastElementChild.className = s.name;
}
function styledDeparArg(s: State)
{
    parseCurly(s);
    s.out.lastElementChild.className = s.name;
    if (s.out instanceof HTMLParagraphElement)
    {
        const result = s.out.lastChild;
        const parent = s.out.parentNode;
        parent.insertBefore(result, s.out);

        if (s.out.firstChild != null)
        {
            const split_par = document.createElement('p');
            while (s.out.firstChild != null)
                split_par.appendChild(s.out.firstChild);
            parent.insertBefore(split_par, result);
        }
    }
}

function parseLabel(s: State)
{
    const id = s.src.readCurly();
    const span = s.appendElement('span');
    span.id = id;
    span.className = 'label';
    span.textContent = id;
}

function parseRef(s: State)
{
    const target = s.src.readCurly();
    const anchor = s.out.appendChild(document.createElement('a'));
    anchor.href = '#' + target;
    anchor.textContent = target;
}

function substitute(args: Node, out: Node)
{
    for (let n = out.firstChild; n != null; n = n.nextSibling)
    {
        if (n instanceof Text)
        {
            const src = new ReadState(n.data, 0);
            while (src.hasNext && src.nextCode != ARGUMENT)
                src.skip();

            if (!src.hasNext)
                continue;

            if (src.next == 0)
            {
                src.skip();
                const start = src.next;
                while (src.hasNext && src.nextIsDigit)
                    src.skip();
                if (src.next == start)
                    throw new Error("expected number after argument sign");
                if (!src.hasNext)
                    n.splitText(src.next);

                const argn = Number(n.data.substring(1));
                const arg = args.childNodes.item(argn - 1);
                if (argn === null)
                    throw new Error('invalid argument number: ' + argn);

                const replacement = arg.cloneNode(true) as HTMLElement;
                replacement.className = 'expanded-arg-' + argn;
                out.replaceChild(replacement, n);
            }
            else if (src.hasNext)
                n.splitText(src.next);
        }
        else
            substitute(args, n as HTMLElement);
    }
}

function parseNewcommand(s: State)
{
    s = s.pushOutput(s.appendElement('div'));
    s.out.appendChild(createTextSpan('command', s.name));

    const cmd = s.src.readCurly();
    if (cmd.charCodeAt(0) != COMMAND)
        throw new Error('first argument to newcommand must begin with \\');
    s.out.appendChild(createTextSpan('curly', cmd));

    const argc = s.src.readSquare();
    if (argc !== null)
    {
        s.out.appendChild(createTextSpan('square', argc));
    }

    parseCurly(s.pushName('newcommand', Mode.math));
    const definition = s.out.lastChild;

    commands[cmd.substring(1)] = (s: State) =>
    {
        const result = s.out.appendChild(definition.cloneNode(true) as HTMLElement);
        result.className = 'expanded-' + cmd.substring(1);

        if (argc != null)
        {
            const temp = s.pushOutput(document.createElement('div'));
            for (let i = 0; i < +argc; i++)
                parseCurly(temp);
            substitute(temp.out, result);
        }
    };
}


function parseBegin(s: State)
{
    const name = s.src.readCurly();
    let mode = Mode.text;
    switch (destar(name))
    {
        case 'math':
        case 'displaymath':
        case 'cases':
        case 'align':
            mode = Mode.math;
    }
    parseEnv(s.pushName(name, mode), 'div');
}

const enum Env { par, table };
const enum Mode { text, math };

const environ: {[index: string]: Env} =
    {
        'align': Env.table,
        'cases': Env.table,
        'document': Env.par,
        'tabular': Env.table,
        'root': Env.par
    };

function allSpace(el: Node)
{
    if (el instanceof Text)
    {
        for (let i = 0; i < el.data.length; i++)
            if (el.data.charCodeAt(i) > SPACE)
                return false;
        return true;
    }
    for (let n = el.firstChild; n != null; n = n.nextSibling)
        if (!allSpace(n))
            return false;
    return true;
}

function parseEnv(s: State, tag: string)
{
    const nostar_name = destar(s.name);
    const env = environ[nostar_name];
    const first_column_align = nostar_name == 'align' ? 'right' : 'left';
    if (nostar_name == 'itemize') tag = 'ul';
    switch (env)
    {
        case Env.table:
            s = s.pushOutput(s.appendElement('table', nostar_name));
            s = s.pushOutput(s.appendElement('tr'));
            s = s.pushOutput(s.appendElement('td', first_column_align));
            break;
        case Env.par:
            s = s.pushOutput(s.appendElement('div', nostar_name))
            s = s.pushOutput(s.appendElement('p'));
            break;
        default:
            s = s.pushOutput(s.appendElement(tag, nostar_name));
            break;
    }

    let prev_newline = false;
    while (s.src.hasNext)
    {
        const ch = s.src.nextCode;
        switch (ch)
        {
            case COMMENT: parseComment(s); break;
            case TABULATE:
                if (env != Env.table)
                    throw new Error('unexpected tabulation');
                s.src.skip();
                s = s.pushOutput(s.out.parentElement.appendChild(document.createElement('td')));
                s.out.className = 'left';
                break;
            case MATHMODE:
                s.src.skip();
                if (s.name == 'short-math')
                    return;
                parseEnv(s.pushName('short-math', Mode.math), 'span');
                break;
            case SUBSCRIPT:
            case SUPERSCRIPT:
                if (s.mode != Mode.math)
                    throw new Error('unexpected sub/superscript outside of math mode');
                parseSubsup(s);
                break;
            case COMMAND:
                const cmd = s.src.readCommand();
                switch (cmd)
                {
                    case '\\':
                        switch (env)
                        {
                            case Env.table:
                                s = s.pushOutput(s.out.parentElement.parentElement.appendChild(document.createElement('tr')));
                                s = s.pushOutput(s.appendElement('td', first_column_align))
                                break;
                            case Env.par:
                                s.appendElement('br');
                                break;
                        }
                        break;
                    case '[': parseEnv(s.pushName('short-displaymath', Mode.math), 'div'); break;
                    case ']': 
                        if (s.name != 'short-displaymath')
                            throw new Error('unexpected \\]');
                        return;
                    case 'end':
                        if (s.name == 'item')
                        {
                            s.src.next -= cmd.length+1;
                            return;
                        }
                        const endof = s.src.readCurly();
                        if (s.name != endof)
                            throw new Error('begin/end mismatch: ' + name + '/' + endof);
                        return;
                    case 'left':
                        if (s.mode != Mode.math)
                            throw new Error('illegal context for \\left');
                        parseEnv(s.pushName('mathspan'), 'span');
                        break;
                    case 'right':
                        if (s.name != 'mathspan')
                            throw new Error('illegal context for \\right');
                        return;
                    case 'item':
                        switch (nostar_name)
                        {
                            case 'itemize': parseEnv(s.pushName('item'), 'li'); break;
                            case 'item': s.src.next -= cmd.length + 1; return;
                            default: console.log('item outside itemize, ignoring'); break;
                        }
                        break;
                    default:
                        parseCommand(s.pushName(cmd));
                        break;
                }
                break;
            case CURLY_IN: parseCurly(s); break;
            case CURLY_OUT:
                if (s.name != 'curly')
                    throw new Error('unexpected end of group');
                s.src.skip();
                return;
            case NEWLINE:
                if (!prev_newline)
                    prev_newline = true;
                else if (prev_newline && env == Env.par && !allSpace(s.out))
                {
                    // make paragraph break
                    s = s.pushOutput(s.out.parentElement.appendChild(document.createElement('p')));
                    prev_newline = false;
                }
                // fallthrough!
            default:
                appendText(s.src.readChar(), s.out);
                if (ch <= SPACE)
                    continue;
                break;
        }
        prev_newline = false;
    }
}


function parseCommand(s: State)
{
    const proc = commands[s.name];
    if (proc !== undefined)
        return proc(s);

    const sym = symbols[s.name];
    if (sym !== undefined)
    {
        appendText(sym, s.out);
        if (s.src.nextCode == SPACE)
            s.src.skip();
        return;
    }

    const cmd = s.out.appendChild(document.createElement('span'));
    cmd.classList.add('command');
    cmd.textContent = s.name;

    return;
}



function parseDocument(text: string)
{
    if (document.body.lastElementChild.classList.contains('root'))
        document.body.removeChild(document.body.lastElementChild);
    parseEnv(new State(new ReadState(text, 0), 'root', Mode.text, document.body), 'div');
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
