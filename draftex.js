var caret = (function () {
    var result = document.createElement('span');
    result.classList.add('caret');
    return result;
})();
function skipSpace(text, offset) {
    if (offset === void 0) { offset = 0; }
    while (offset < text.length && text.charCodeAt(offset) <= 32)
        offset++;
    return offset;
}
function isAlpha(ch) {
    return (ch > 64 && ch < 91) || (ch > 96 && ch < 123);
}
function isDigit(ch) {
    return (ch >= 48 && ch < 58);
}
function destar(name) {
    return name.replace('*', '');
}
function appendText(text, out) {
    if (out.lastChild == null || out.lastChild.nodeType != Node.TEXT_NODE)
        out.appendChild(document.createTextNode(text));
    else
        out.lastChild.appendData(text);
}
var commands = {
    'begin': parseBegin,
    'newcommand': parseNewcommand,
    'title': styledArg,
    'author': styledArg,
    'section': styledArg,
    'subsection': styledArg,
    'subsubsection': styledArg,
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
var symbols = {
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
};
var SPACE = ' '.charCodeAt(0);
var COMMAND = '\\'.charCodeAt(0);
var COMMENT = '%'.charCodeAt(0);
var ARGUMENT = '#'.charCodeAt(0);
var MATHMODE = '$'.charCodeAt(0);
var TABULATE = '&'.charCodeAt(0);
var NEWLINE = '\n'.charCodeAt(0);
var CURLY_IN = '{'.charCodeAt(0);
var CURLY_OUT = '}'.charCodeAt(0);
var SQUARE_IN = '['.charCodeAt(0);
var SQUARE_OUT = ']'.charCodeAt(0);
var SUBSCRIPT = '_'.charCodeAt(0);
var SUPERSCRIPT = '^'.charCodeAt(0);
function fixedFromCharCode(codePt) {
    if (codePt > 0xFFFF) {
        codePt -= 0x10000;
        return String.fromCharCode(0xD800 + (codePt >> 10), 0xDC00 + (codePt & 0x3FF));
    }
    else {
        return String.fromCharCode(codePt);
    }
}
var mathbb_lookup = {
    'C': '\u2102',
    'H': '\u210d',
    'N': '\u2115',
    'P': '\u2119',
    'Q': '\u211a',
    'R': '\u211d',
    'Z': '\u2124'
};
function parseMathbb(text, next, name, out) {
    var src = readCurly(text, next);
    var dst = out.appendChild(document.createElement('span'));
    dst.className = 'mathbb';
    for (var i = 0; i < src.length; i++) {
        var lu = mathbb_lookup[src.charAt(i)];
        if (lu !== undefined)
            appendText(lu, dst);
        else
            appendText(fixedFromCharCode(src.charCodeAt(i) + (0x1d538 - 0x41)), dst);
    }
    return next + src.length + 2;
}
var mathcal_lookup = {
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
};
function parseMathcal(text, next, name, out) {
    var src = readCurly(text, next);
    var dst = out.appendChild(document.createElement('span'));
    dst.className = 'mathcal';
    for (var i = 0; i < src.length; i++) {
        var lu = mathcal_lookup[src.charAt(i)];
        if (lu !== undefined)
            appendText(lu, dst);
        else
            appendText(fixedFromCharCode(src.charCodeAt(i) + (0x1d49c - 0x41)), dst);
    }
    return next + src.length + 2;
}
function parseFraction(text, next, name, out) {
    var table = out.appendChild(document.createElement('div'));
    table.className = 'frac';
    next = parseCurly(text, next, table);
    table.appendChild(document.createElement('hr'));
    next = parseCurly(text, next, table);
    table.firstElementChild.className = 'numerator';
    table.lastElementChild.className = 'denominator';
    return next;
}
function parseComment(text, next, out) {
    var last = next;
    while (text.charCodeAt(last) != NEWLINE)
        last++;
    var comment = out.appendChild(document.createElement('span'));
    comment.classList.add('comment');
    comment.textContent = text.substring(next, last);
    return last;
}
function createSpan(classname, content) {
    var result = document.createElement('span');
    result.classList.add(classname);
    result.appendChild(content);
    return result;
}
function createTextSpan(classname, content) {
    return createSpan(classname, document.createTextNode(content));
}
function readSquare(text, next) {
    if (next >= text.length || text.charCodeAt(next) != SQUARE_IN)
        return null;
    next++;
    var end = text.indexOf(']', next);
    if (end >= text.length)
        throw new Error('missing right square bracket');
    return text.substring(next, end);
}
function readCurly(text, next) {
    if (next >= text.length || text.charCodeAt(next) != CURLY_IN)
        throw new Error('expected group');
    next++;
    var end = text.indexOf('}', next);
    if (end >= text.length)
        throw new Error('missing group end');
    return text.substring(next, end);
}
function parseCurly(text, next, out) {
    if (next >= text.length || text.charCodeAt(next) != CURLY_IN)
        throw new Error('expected group');
    next++;
    return parseEnv(text, next, 'curly', 'span', out);
}
function parseSubsup(text, next, out) {
    var sub = out.appendChild(document.createElement(text.charCodeAt(next) == SUPERSCRIPT ? 'sup' : 'sub'));
    next++;
    if (next >= text.length)
        throw new Error('expected more characters after subscript');
    switch (text.charCodeAt(next)) {
        case CURLY_IN:
            next = parseCurly(text, next, sub);
            // unpack braces
            for (var n = sub.firstElementChild.firstChild; n != null; n = n.nextSibling)
                sub.appendChild(n);
            sub.firstElementChild.remove();
            break;
        case COMMAND:
            var cmd = readCommand(text, next + 1);
            next = next + 1 + cmd.length;
            next = parseCommand(text, next, cmd, sub);
            break;
        default:
            sub.textContent = text.charAt(next);
            next++;
            break;
    }
    return next;
}
function styledArg(text, next, name, out) {
    next = parseCurly(text, next, out);
    out.lastElementChild.className = name;
    return next;
}
function parseLabel(text, next, name, out) {
    var id = readCurly(text, next);
    var span = out.appendChild(document.createElement('span'));
    span.id = id;
    span.className = 'label';
    span.textContent = id;
    return next + id.length + 2;
}
function parseRef(text, next, name, out) {
    var target = readCurly(text, next);
    var anchor = out.appendChild(document.createElement('a'));
    anchor.href = '#' + target;
    anchor.textContent = target;
    return next + target.length + 2;
}
function substitute(args, out) {
    for (var n = out.firstChild; n != null; n = n.nextSibling) {
        if (n.nodeType == Node.TEXT_NODE) {
            var text = n;
            var off = text.data.indexOf('#');
            if (off == -1)
                continue;
            if (off == 0) {
                var end = 1;
                while (end < text.data.length && isDigit(text.data.charCodeAt(end)))
                    end++;
                if (end == 1)
                    throw new Error("expected number after argument sign");
                if (end < text.data.length)
                    text.splitText(end);
                var replacement = args.childNodes.item(+text.data).cloneNode(true);
                replacement.className = 'expanded-arg-' + text.data.substring(1);
                out.replaceChild(replacement, text);
            }
            else if (off < text.data.length)
                text.splitText(off);
        }
        else
            substitute(args, n);
    }
}
function parseNewcommand(text, next, name, out) {
    var div = out.appendChild(document.createElement('div'));
    div.appendChild(createTextSpan('command', name));
    var cmd = readCurly(text, next);
    next = next + cmd.length + 2;
    div.appendChild(createTextSpan('curly', cmd));
    var argc = readSquare(text, next);
    if (argc !== null) {
        next = next + argc.length + 2;
        div.appendChild(createTextSpan('square', argc));
    }
    next = parseCurly(text, next, div);
    if (cmd.charCodeAt(0) != COMMAND)
        throw new Error('first argument to newcommand must begin with \\');
    if (argc != null) {
        commands[cmd.substring(1)] = function (text, next, name, out) {
            var result = out.appendChild(div.lastChild.cloneNode(true));
            result.className = 'expanded-' + cmd.substring(1);
            var temp = document.createElement('div');
            for (var i = 0; i < +argc; i++)
                next = parseCurly(text, next, temp);
            substitute(temp, result);
            return next;
        };
    }
    else {
        commands[cmd.substring(1)] = function (text, next, name, out) {
            out.appendChild(div.lastChild.cloneNode(true))
                .className = 'expanded-' + cmd.substring(1);
            return next;
        };
    }
    return next;
}
function parseBegin(text, next, name, out) {
    var sub_name = readCurly(text, next);
    return parseEnv(text, next + sub_name.length + 2, sub_name, 'div', out);
}
function parseEnv(text, next, name, tag, out) {
    if (name == 'itemize')
        tag = 'ul';
    if (destar(name) == 'align' || name == 'tabular' || name == 'cases') {
        out = out.appendChild(document.createElement('table'));
        out = out.appendChild(document.createElement('tr'));
        tag = 'td';
    }
    var env = out.appendChild(document.createElement(tag));
    if (destar(name) == 'align' || name == 'tabular' || name == 'cases') {
        env.parentElement.parentElement.className = destar(name);
        if (destar(name) == 'align')
            env.className = 'right';
        else
            env.className = 'left';
    }
    else
        env.classList.add(destar(name));
    while (next < text.length) {
        var ch = text.charCodeAt(next);
        switch (ch) {
            case COMMENT:
                next = parseComment(text, next + 1, env);
                break;
            case TABULATE:
                next = next + 1;
                env = env.parentElement.appendChild(document.createElement('td'));
                env.className = 'left';
                break;
            case MATHMODE:
                next++;
                if (name == 'short-math')
                    return next;
                next = parseEnv(text, next, 'short-math', 'span', env);
                break;
            case SUBSCRIPT:
            case SUPERSCRIPT:
                next = parseSubsup(text, next, env);
                break;
            case COMMAND:
                var cmd = readCommand(text, next + 1);
                next = next + 1 + cmd.length;
                switch (cmd) {
                    case '\\':
                        env = env.parentElement.parentElement.appendChild(document.createElement('tr'));
                        env = env.appendChild(document.createElement('td'));
                        if (destar(name) == 'align')
                            env.className = 'right';
                        else
                            env.className = 'left';
                        break;
                    case '[':
                        next = parseEnv(text, next, 'short-displaymath', 'div', env);
                        break;
                    case ']':
                        if (name != 'short-displaymath')
                            throw new Error('unexpected \\]');
                        return next;
                    case 'end':
                        if (name == 'item')
                            return next - (cmd.length + 1);
                        var endof = readCurly(text, next);
                        next = next + endof.length + 2;
                        if (name != endof)
                            throw new Error('begin/end mismatch: ' + name + '/' + endof);
                        return next;
                    case 'left':
                        next = parseEnv(text, next, 'mathspan', 'span', env);
                        break;
                    case 'right':
                        if (name != 'mathspan')
                            throw new Error('illegal context for \\right');
                        return next;
                    case 'item':
                        switch (name) {
                            case 'itemize':
                                next = parseEnv(text, next, 'item', 'li', env);
                                break;
                            case 'item': return next - (cmd.length + 1);
                            default:
                                console.log('item outside itemize, ignoring');
                                break;
                        }
                        break;
                    default:
                        next = parseCommand(text, next, cmd, env);
                        break;
                }
                break;
            case CURLY_IN:
                next = parseCurly(text, next, env);
                break;
            case CURLY_OUT:
                if (name != 'curly')
                    throw new Error('unexpected end of group');
                return next + 1;
            default:
                appendText(text.charAt(next), env);
                next++;
                break;
        }
    }
    return next;
}
function readCommand(text, next) {
    if (isAlpha(text.charCodeAt(next))) {
        var last = next;
        while (last < text.length && isAlpha(text.charCodeAt(last)))
            last++;
        return text.substring(next, last);
    }
    return text.charAt(next);
}
function parseCommand(text, next, name, out) {
    var proc = commands[name];
    if (proc !== undefined)
        return proc(text, next, name, out);
    var sym = symbols[name];
    if (sym !== undefined) {
        appendText(sym, out);
        if (text.charCodeAt(next) == SPACE)
            next++;
        return next;
    }
    var cmd = out.appendChild(document.createElement('span'));
    cmd.classList.add('command');
    cmd.textContent = name;
    return next;
}
function parseDocument(text) {
    if (document.body.lastElementChild.classList.contains('root'))
        document.body.removeChild(document.body.lastElementChild);
    parseEnv(text, 0, 'root', 'div', document.body);
    //let env = new Env('root', text, 0);
    //document.body.appendChild(env.element);
}
function parseUri(uri) {
    fetch(uri, { cache: 'no-store' })
        .then(function (response) { return response.text(); })
        .then(parseDocument);
}
function parseLocal(event) {
    var reader = new FileReader();
    reader.onload = function () { return parseDocument(reader.result); };
    reader.readAsText(event.target.files[0]);
}
function stepright() {
    if (caret.nextSibling != null) {
        if (caret.nextSibling.nodeType == Node.TEXT_NODE) {
            var spacec = skipSpace(caret.nextSibling.textContent);
            var split_pos = caret.nextSibling.textContent.length - (spacec == 0 ? 1 : spacec);
            if (split_pos > 0) {
                caret.parentElement.insertBefore(caret.nextSibling.splitText(split_pos), caret);
                caret.parentElement.normalize();
            }
            else {
                caret.parentElement.insertBefore(caret.nextSibling, caret);
            }
        }
        console.log('prev:', caret.previousSibling);
        console.log('next:', caret.nextSibling);
    }
    else if (caret.parentNode != null) {
        console.log('parent:', caret.parentNode);
    }
}
function keydown(event) {
    console.log(event);
    switch (event.code) {
        case "ArrowRight":
            stepright();
            return;
    }
}
function main() {
    var selector = document.createElement('input');
    selector.type = 'file';
    document.body.appendChild(selector);
    selector.onchange = parseLocal;
    document.body.onkeydown = keydown;
    var args = window.location.search.replace('?', '').split('&');
    for (var i = 0; i < args.length; i++) {
        var arg = args[i].split('=');
        if (arg.length == 2 && arg[0] == 'src')
            parseUri(arg[1]);
    }
}
//# sourceMappingURL=draftex.js.map