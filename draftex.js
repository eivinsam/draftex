var root = document.createElement('div');
function isAlpha(ch) {
    return (ch > 64 && ch < 91) || (ch > 96 && ch < 123);
}
var argc = {
    'documentclass': 1,
    'usepackage': 1,
    'setcounter': 2,
    'newcommand': 2,
    'begin': 1,
    'end': 1,
    'usetikzlibrary': 1,
    'acmConference': 4,
    'acmYear': 1,
    'copyrightyear': 1,
    'title': 1,
    'author': 1,
    'affiliation': 1,
    'email': 1
};
function getBlock(text, offset) {
    offset++;
    var start = offset;
    var depth = 1;
    while (offset < text.length && depth > 0) {
        switch (text.charAt(offset)) {
            case '{':
                depth++;
                break;
            case '}':
                depth--;
                break;
            case ']':
                if (depth == 1)
                    depth--;
                break;
        }
        offset++;
    }
    return text.substring(start, offset - 1);
}
var Command = /** @class */ (function () {
    function Command(text, offset) {
        this.start = offset;
        while (isAlpha(text.charCodeAt(offset)))
            offset++;
        this.name = text.substring(this.start, offset);
        this.args = [];
        while (offset < text.length && text.charAt(offset) == ' ')
            offset++;
        if (text.charAt(offset) == '[') {
            this.optarg = getBlock(text, offset);
            offset = offset + this.optarg.length + 2;
        }
        while (offset < text.length && text.charAt(offset) == ' ')
            offset++;
        console.log(this.name + '[' + argc[this.name] + ']');
        for (var rem = argc[this.name]; rem > 0; rem--) {
            if (text.charAt(offset) != '{' && text.charAt(offset) != '[') {
                console.log('too few arguments to command \\' + this.name);
                return;
            }
            var contents = getBlock(text, offset);
            this.args.push(contents);
            offset = offset + contents.length + 2;
            if (text.charAt(offset) == '[')
                rem++;
        }
        this.end = offset - 1;
    }
    return Command;
}());
var Env = /** @class */ (function () {
    function Env(name, text, end) {
        this.end = end;
        console.log('entering environment ' + name);
        this.element = document.createElement('div');
        this.element.classList.add(name);
        var newline = false;
        for (; this.end < text.length && this.end < 900; this.end++) {
            var first = text.charAt(this.end);
            switch (first) {
                case '%':
                    this.end = text.indexOf('\n');
                    break;
                case '\\':
                    if (newline)
                        root.appendChild(document.createElement('br'));
                    var cmd = new Command(text, this.end + 1);
                    this.end = cmd.end;
                    if (cmd.name == '[') {
                        var sub = new Env('[', text, this.end);
                        this.end = sub.end;
                        this.element.appendChild(sub.element);
                    }
                    else if (cmd.name == ']') {
                        if (name == '[')
                            return;
                        console.log('unexpected end of math mode');
                    }
                    else if (cmd.name == 'begin') {
                        var sub = new Env(cmd.args[0], text, this.end + 1);
                        console.log('exiting evironment ' + cmd.args[0]);
                        this.end = sub.end;
                        this.element.appendChild(sub.element);
                    }
                    else if (cmd.name == 'end') {
                        if (name == 'root')
                            console.log('too many ends');
                        else if (name != cmd.args[0])
                            console.log('begin/end mismatch');
                        else
                            return;
                    }
                    else {
                        if (newline)
                            this.element.appendChild(document.createElement('br'));
                        var cmde = document.createElement('span');
                        cmde.classList.add('command');
                        cmde.textContent = '\\' + cmd.name;
                        if (cmd.optarg)
                            cmde.textContent = cmde.textContent + '[' + cmd.optarg + ']';
                        for (var i = 0; i < cmd.args.length; i++)
                            cmde.textContent = cmde.textContent + '{' + cmd.args[i] + '}';
                        this.element.appendChild(cmde);
                    }
                    break;
                case '$':
                    if (name == '$')
                        return;
                    else {
                        var sub = new Env('$', text, this.end + 1);
                        this.end = sub.end;
                        this.element.appendChild(sub.element);
                    }
                    break;
                case '{':
                    {
                        var sub = new Env('block', text, this.end + 1);
                        this.end = sub.end;
                        this.element.appendChild(sub.element);
                    }
                    break;
                case '}':
                    if (name == 'block')
                        return;
                    console.log('curly brace mismatch');
                    break;
                case '\n':
                    newline = true;
                    continue;
                default:
                    //root.textContent = root.textContent + first;
                    break;
            }
            newline = false;
        }
    }
    return Env;
}());
function parseDocument(text) {
    var env = new Env('root', text, 0);
    if (document.body.childElementCount > 1)
        document.body.removeChild(document.body.lastElementChild);
    document.body.appendChild(env.element);
}
function parse(event) {
    var reader = new FileReader();
    reader.onload = function () { return parseDocument(reader.result); };
    reader.readAsText(event.target.files[0]);
}
function main() {
    var selector = document.createElement('input');
    selector.type = 'file';
    document.body.appendChild(selector);
    selector.onchange = parse;
}
//# sourceMappingURL=draftex.js.map