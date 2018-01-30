﻿const root = document.createElement('div');

function isAlpha(ch: number)
{
    return (ch > 64 && ch < 91) || (ch > 96 && ch < 123);
}

const argc =
    {
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

function getBlock(text: string, offset: number)
{
    offset++;
    const start = offset;
    let depth = 1;
    while (offset < text.length && depth > 0)
    {
        switch (text.charAt(offset))
        {
            case '{': depth++; break;
            case '}': depth--; break;
            case ']':
                if (depth == 1)
                    depth--;
                break;
        }
        offset++;
    }
    return text.substring(start, offset - 1);
}

class Command 
{
    readonly start: number;
    readonly end: number;
    readonly name: string;
    readonly args: string[];
    readonly optarg: string;

    constructor(text: string, offset: number)
    {
        this.start = offset;
        while (isAlpha(text.charCodeAt(offset))) offset++;
        this.name = text.substring(this.start, offset);
        this.args = [];

        while (offset < text.length && text.charAt(offset) == ' ') offset++
        if (text.charAt(offset) == '[')
        {
            this.optarg = getBlock(text, offset);
            offset = offset + this.optarg.length + 2;
        }
        while (offset < text.length && text.charAt(offset) == ' ') offset++;
        console.log(this.name + '[' + argc[this.name] + ']');
        for (let rem = argc[this.name]; rem > 0; rem--)
        {
            if (text.charAt(offset) != '{' && text.charAt(offset) != '[')
            {
                console.log('too few arguments to command \\' + this.name);
                return;
            }
            const contents = getBlock(text, offset);
            this.args.push(contents);
            offset = offset + contents.length + 2;
            if (text.charAt(offset) == '[')
                rem++;
        }
        this.end = offset - 1;
    }
}


class Env
{
    readonly start: number;
    readonly element: HTMLElement;

    constructor(name: string, text: string, public readonly end: number)
    {
        console.log('entering environment ' + name);
        this.element = document.createElement('div');
        this.element.classList.add(name);

        let newline = false;
        for ( ; this.end < text.length && this.end < 900; this.end++)
        {
            const first = text.charAt(this.end);
            switch (first)
            {
                case '%': this.end = text.indexOf('\n'); break;
                case '\\':
                    if (newline)
                        root.appendChild(document.createElement('br'));
                    const cmd = new Command(text, this.end+1);
                    this.end = cmd.end;
                    if (cmd.name == '[')
                    {
                        const sub = new Env('[', text, this.end);
                        this.end = sub.end;
                        this.element.appendChild(sub.element);

                    }
                    else if (cmd.name == ']')
                    {
                        if (name == '[')
                            return;
                        console.log('unexpected end of math mode');
                    }
                    else if (cmd.name == 'begin')
                    {
                        const sub = new Env(cmd.args[0], text, this.end + 1);
                        console.log('exiting evironment ' + cmd.args[0]);
                        this.end = sub.end;
                        this.element.appendChild(sub.element);
                    }
                    else if (cmd.name == 'end')
                    {
                        if (name == 'root')
                            console.log('too many ends');
                        else if (name != cmd.args[0])
                            console.log('begin/end mismatch');
                        else
                            return;
                    }
                    else
                    {
                        if (newline)
                            this.element.appendChild(document.createElement('br'));
                        const cmde = document.createElement('span');
                        cmde.classList.add('command');
                        cmde.textContent = '\\' + cmd.name;
                        if (cmd.optarg)
                            cmde.textContent = cmde.textContent + '[' + cmd.optarg + ']';
                        for (let i = 0; i < cmd.args.length; i++)
                            cmde.textContent = cmde.textContent + '{' + cmd.args[i] + '}';
                        this.element.appendChild(cmde);
                    }
                    break;
                case '$':
                    if (name == '$')
                        return;
                    else
                    {
                        const sub = new Env('$', text, this.end + 1);
                        this.end = sub.end;
                        this.element.appendChild(sub.element);
                    }
                    break;
                case '{':
                    {
                        const sub = new Env('block', text, this.end + 1);
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
}




function parseDocument(text: string)
{
    let env = new Env('root', text, 0);
    if (document.body.childElementCount > 1)
        document.body.removeChild(document.body.lastElementChild);
    document.body.appendChild(env.element);
}

function parse(event)
{
    const reader = new FileReader();
    reader.onload = () => parseDocument(reader.result);
    reader.readAsText(event.target.files[0]);
}

function main()
{
    const selector = document.createElement('input');
    selector.type = 'file';

    document.body.appendChild(selector);

    selector.onchange = parse;
}
