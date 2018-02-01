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
    if (cmd.textContent == '[') {
        rest.removeChild(cmd);
        target.appendChild(collectEnviron(rest, 'short-displaymath', 'div'));
    }
    else if (cmd.textContent == ']') {
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
    if (name == 'document')
        target.appendChild(caret);
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
        var _this = this;
        this.name = name;
        var is_bracket = name.substring(0, 7) == 'bracket';
        this.start = offset;
        this.end = offset + (is_bracket ? 1 : 0);
        this.element = document.createElement(is_bracket ? 'span' : 'div');
        this.element.classList.add(name);
        var push_char = function (ch) {
            if (_this.element.lastChild != null && _this.element.lastChild.nodeType == Node.TEXT_NODE)
                _this.element.lastChild.textContent = _this.element.lastChild.textContent + ch;
            else
                _this.element.appendChild(document.createTextNode(ch));
        };
        var par = null;
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
                    if (cmd_name == '[') {
                        var sub_2 = new Env('short-displaymath', text, this.end);
                        this.end = sub_2.end;
                        this.element.appendChild(sub_2.element);
                    }
                    else if (cmd_name == ']') {
                        if (name != 'short-displaymath')
                            console.log('\\[ / \\] mismatch');
                        break outer_loop;
                    }
                    else {
                        this.element.appendChild(document.createElement('span'));
                        this.element.lastElementChild.classList.add('command');
                        this.element.lastElementChild.textContent = cmd_name;
                    }
                    break;
                default:
                    if (ch.charCodeAt(0) <= 32) {
                        var spaces = text.substring(this.end - 1, skipSpace(text, this.end));
                        this.end = this.end + spaces.length - 1;
                        var newlinec = 0;
                        for (var i = 0; i < spaces.length; i++)
                            if (spaces.charAt(i) == '\n')
                                newlinec++;
                        if (newlinec >= 2 && name != 'itemize') {
                            // push paragraph
                            var first = par == null ? this.element.firstChild : par.nextSibling;
                            while (first != null && first.nodeType != Node.TEXT_NODE)
                                first = first.nextSibling;
                            if (first == null)
                                continue;
                            par = document.createElement('p');
                            this.element.insertBefore(par, first);
                            var last = this.element.lastChild;
                            while (last.nodeType != Node.TEXT_NODE)
                                last = last.previousSibling;
                            while (first != last) {
                                var new_first = first.nextSibling;
                                par.appendChild(first);
                                first = new_first;
                            }
                            par.appendChild(last);
                            par = collectEnviron(par, name, par.tagName);
                            this.element.appendChild(par);
                        }
                        else if (this.element.lastChild != null && this.element.lastChild.nodeType == Node.TEXT_NODE)
                            push_char(' ');
                    }
                    else
                        push_char(ch);
                    break;
            }
            var envcmd = this.element.lastChild == null ? null : this.element.lastChild.previousSibling;
            if (envcmd != null && envcmd.nodeType == Node.ELEMENT_NODE && envcmd.classList.contains('command')) {
                if (envcmd.textContent == 'begin') {
                    var arg = this.element.lastChild;
                    if (arg.nodeType != Node.ELEMENT_NODE || !arg.classList.contains('bracket-curly'))
                        throw new Error('argument expected after \\begin');
                    this.element.removeChild(arg);
                    this.element.removeChild(envcmd);
                    var sub = new Env(arg.textContent, text, this.end);
                    this.end = sub.end;
                    this.element.appendChild(sub.element);
                }
                else if (envcmd.textContent == 'end') {
                    var arg = this.element.lastChild;
                    if (!arg.classList.contains('bracket-curly'))
                        throw new Error('argument expected after \\end');
                    this.element.removeChild(arg);
                    this.element.removeChild(envcmd);
                    break;
                }
            }
        }
        this.element = collectEnviron(this.element, name, this.element.tagName);
        //else
        //{
        //    const last_par = document.createElement('p');
        //    for (let n = par.nextSibling; n != null; n = par.nextSibling)
        //        last_par.appendChild(n);
        //    this.element.appendChild(last_par);
        //    for (let n = this.element.firstElementChild as HTMLElement; n != null; n = n.nextElementSibling as HTMLElement)
        //    {
        //        const new_n = collectEnviron(n, name, n.tagName);
        //        this.element.replaceChild(new_n, n);
        //        n = new_n;
        //    }
        //}
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