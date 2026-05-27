\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{OS_experiment_report}[2026/03/23 SDU OS Report Template]

\LoadClass[12pt, a4paper]{article}

% ---------- 核心包 ----------
\RequirePackage[UTF8]{ctex}
\RequirePackage{geometry}
\RequirePackage{graphicx}
\RequirePackage{float}
\RequirePackage{hyperref}
\RequirePackage{fancyhdr}
\RequirePackage{parskip}
\RequirePackage{listings}
\RequirePackage{xcolor}

% ---------- 页面设置 ----------
\geometry{left=2.5cm, right=2.5cm, top=3cm, bottom=3cm, headheight=18pt}
\setlength{\parindent}{2em}
\setlength{\parskip}{0.5em}

% ---------- 变量定义 ----------
\newcommand{\course}[1]{\def\@course{#1}}
\newcommand{\expName}[1]{\def\@expName{#1}}
\newcommand{\stuName}[1]{\def\@stuName{#1}}
\newcommand{\stuID}[1]{\def\@stuID{#1}}
\newcommand{\major}[1]{\def\@major{#1}}
\newcommand{\logoPath}[1]{\def\@logoPath{#1}}

\course{操作系统实验报告}
\logoPath{figure/logo_sduWithText.jpg}

% ---------- 页眉页脚 ----------
\pagestyle{fancy}
\fancyhf{}
\fancyhead[L]{\small \textbf{\@course}}
\fancyhead[C]{\small \textbf{\@expName}}
\fancyhead[R]{\small \@stuName}
\fancyfoot[C]{\thepage}
\renewcommand{\headrulewidth}{0.4pt}

% ---------- 代码样式优化 (支持中文注释) ----------
\definecolor{commentgreen}{rgb}{0,0.6,0}
\definecolor{keywordblue}{rgb}{0,0,0.8}

\lstset{
    basicstyle=\ttfamily\small,
    breaklines=true,
    frame=single,
    numbers=left,
    numberstyle=\tiny\color{gray},
    keywordstyle=\color{keywordblue}\bfseries,
    commentstyle=\color{commentgreen}\itshape,
    stringstyle=\color{orange},
    keepspaces=true,
    extendedchars=false,
    escapeinside=``,
    showstringspaces=false,
    xleftmargin=1em,
    xrightmargin=1em
}

% 针对 C/C++ 的专用命令
\lstnewenvironment{ccode}[1]
{\lstset{language=C++, title={#1}}}
{}

% 针对 汇编 的专用命令
\lstnewenvironment{ascode}[1]
{\lstset{language=[x86masm]Assembler, title={#1}}}
{}

% ---------- 信息栏辅助 ----------
% 自动换行字段（解决长学号重叠问题）
\newcommand{\infofield}[1]{%
    \parbox[t]{8cm}{%
        \centering
        #1\\[-0.3em]
        \rule{8cm}{0.4pt}
    }%
}

% ---------- 封面 ----------
\renewcommand{\maketitle}{
    \begin{titlepage}
        \thispagestyle{empty}
        \centering

        \vspace*{0cm}

        \ifx\@logoPath\undefined
        \else
            \includegraphics[width=0.5\textwidth]{\@logoPath}
        \fi

        \par\vspace{1.5cm}

        {\heiti\zihao{2} \@course \par}

        \vspace{3cm}

        \renewcommand{\arraystretch}{2}

        \zihao{4}

        \begin{tabular}{rl}
            姓名： &
            \infofield{\@stuName}
            \\[1.2em]

            学号： &
            \infofield{\@stuID}
            \\[1.2em]

            专业： &
            \infofield{\@major}
            \\[1.2em]

            日期： &
            \infofield{\today}
            \\
        \end{tabular}

        \vfill
    \end{titlepage}
}