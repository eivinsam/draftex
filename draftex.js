var root = document.createElement('div');
function isAlpha(ch) {
    return (ch > 64 && ch < 91) || (ch > 96 && ch < 123);
}
function destar(name) {
    return name.replace('*', '');
}
function styleUnary(source, args, tag) {
    if (tag === void 0) { tag = 'div'; }
    var result = document.createElement(tag);
    result.classList.add(destar(source.textContent));
    var arg = args.firstChild;
    if (arg != null && arg.nodeType == Node.ELEMENT_NODE && arg.classList.contains('bracket-curly')) {
        args.removeChild(arg);
        result.textContent = arg.textContent;
    }
    return result;
}
function handleCommand(cmd, rest, target) {
    if (cmd.textContent == 'begin') {
        rest.removeChild(cmd);
        var a = rest.firstChild;
        if (a.nodeType == Node.ELEMENT_NODE && a.classList.contains('bracket-curly')) {
            rest.removeChild(a);
            target.appendChild(collectEnviron(rest, a.textContent, 'div'));
        }
        else
            console.log('expected arguent after \\begin');
        return true;
    }
    else if (cmd.textContent == '[') {
        rest.removeChild(cmd);
        target.appendChild(collectEnviron(rest, 'short-displaymath', 'div'));
    }
    else if (cmd.textContent == 'end' || cmd.textContent == ']') {
        return false;
    }
    else {
        rest.removeChild(cmd);
        var style = styles[destar(cmd.textContent)];
        if (style !== undefined) {
            target.appendChild(style(cmd, rest));
        }
        else {
            var sym = symbols[cmd.textContent];
            if (sym !== undefined)
                target.appendChild(document.createTextNode(sym));
            else
                target.appendChild(cmd);
        }
    }
    return true;
}
function styleItem(source, args) {
    var result = document.createElement('li');
    for (var n = args.firstChild; n != null; n = args.firstChild) {
        var e = n;
        if (e.nodeType == Node.ELEMENT_NODE && e.classList.contains('command')) {
            if (e.textContent == 'item' || !handleCommand(e, args, result))
                break;
        }
        else {
            args.removeChild(n);
            result.appendChild(n);
        }
    }
    return result;
}
function styleLabel(source, args) {
    var result = styleUnary(source, args);
    result.id = result.textContent;
    return result;
}
function styleRef(source, args) {
    var result = styleUnary(source, args, 'a');
    result.href = '#' + result.textContent;
    return result;
}
var styles = {
    'item': styleItem,
    'label': styleLabel,
    'ref': styleRef,
    'autoref': styleRef,
    'title': styleUnary,
    'author': styleUnary,
    'section': styleUnary,
    'subsection': styleUnary,
    'caption': styleUnary,
    'cite': styleUnary,
    'text': styleUnary,
    'floor': styleUnary,
    'ceil': styleUnary
};
var symbols = {
    'Delta': 'Δ',
    'phi': 'ϕ',
    'sigma': 'σ',
    'times': '×',
    'prec': "\u227A",
    'succ': "\u227B",
    'lbrace': '{',
    'rbrace': '}'
};
function collectEnviron(source, name, type) {
    var target = document.createElement(name == 'itemize' ? 'ul' : type);
    target.classList.add(name);
    for (var n = source.firstChild; n != null; n = source.firstChild) {
        var e = n;
        if (e.nodeType == Node.ELEMENT_NODE && e.classList.contains('command')) {
            if (!handleCommand(e, source, target)) {
                source.removeChild(source.firstChild);
                if (e.textContent == ']') {
                }
                else {
                    var a = source.firstChild;
                    if (a.nodeType == Node.ELEMENT_NODE && a.classList.contains('bracket-curly')) {
                        source.removeChild(a);
                        if (a.textContent != name)
                            console.log('begin/end mismatch: ' + name + '/' + a.textContent);
                    }
                    else
                        console.log('expected argument after \\end');
                }
                return target;
            }
        }
        else {
            source.removeChild(n);
            target.appendChild(n);
        }
    }
    return target;
}
var Env = /** @class */ (function () {
    function Env(name, text, offset) {
        this.name = name;
        var is_bracket = name.substring(0, 7) == 'bracket';
        this.start = offset;
        this.end = offset + (is_bracket ? 1 : 0);
        this.element = document.createElement(is_bracket ? 'span' : 'div');
        this.element.classList.add(name);
        outer_loop: while (this.end < text.length) {
            var ch = text.charAt(this.end);
            this.end++;
            switch (ch) {
                case '%':
                    var comment = text.substring(this.end, text.indexOf('\n', this.end));
                    this.element.appendChild(document.createElement('span'));
                    this.element.lastElementChild.classList.add('comment');
                    this.element.lastElementChild.textContent = comment;
                    this.end = this.end + comment.length;
                    break;
                case '$':
                    if (name == 'short-math')
                        break outer_loop;
                    var sub = new Env('short-math', text, this.end);
                    this.end = sub.end;
                    this.element.appendChild(sub.element);
                    break;
                case '}':
                    if (name != 'bracket-curly')
                        console.log('right bracket type mismatch');
                    break outer_loop;
                case ']':
                    if (name != 'bracket-square')
                        console.log('right bracket type mismatch');
                    break outer_loop;
                case '{':
                case '[':
                    {
                        var sub_1 = new Env(ch == '[' ? 'bracket-square' : 'bracket-curly', text, this.end - 1);
                        this.end = sub_1.end;
                        this.element.appendChild(sub_1.element);
                        break;
                    }
                case '\\':
                    var cmd_name = text.charAt(this.end);
                    this.end++;
                    if (isAlpha(cmd_name.charCodeAt(0)))
                        for (; this.end < text.length && (isAlpha(text.charCodeAt(this.end)) || text.charAt(this.end) == '*'); this.end++)
                            cmd_name = cmd_name + text.charAt(this.end);
                    this.element.appendChild(document.createElement('span'));
                    this.element.lastElementChild.classList.add('command');
                    this.element.lastElementChild.textContent = cmd_name;
                    break;
                default:
                    {
                        if (this.element.lastChild != null && this.element.lastChild.nodeType == Node.TEXT_NODE)
                            this.element.lastChild.textContent = this.element.lastChild.textContent + ch;
                        else
                            this.element.appendChild(document.createTextNode(ch));
                    }
                    break;
            }
        }
        this.element = collectEnviron(this.element, name, this.element.tagName);
    }
    return Env;
}());
function parseDocument(text) {
    var env = new Env('root', text, 0);
    if (document.body.childElementCount > 1)
        document.body.removeChild(document.body.lastElementChild);
    document.body.appendChild(env.element);
}
function parseUri(uri) {
    fetch(uri)
        .then(function (response) { return response.text(); })
        .then(parseDocument);
}
function parseLocal(event) {
    var reader = new FileReader();
    reader.onload = function () { return parseDocument(reader.result); };
    reader.readAsText(event.target.files[0]);
}
function main() {
    var selector = document.createElement('input');
    selector.type = 'file';
    document.body.appendChild(selector);
    selector.onchange = parseLocal;
    var args = window.location.search.replace('?', '').split('&');
    for (var i = 0; i < args.length; i++) {
        var arg = args[i].split('=');
        if (arg.length == 2 && arg[0] == 'src')
            parseUri(arg[1]);
    }
}
//# sourceMappingURL=draftex.js.map