const vscode = require('vscode');
const { exec } = require('child_process');
const path = require('path');
const fs = require('fs');

let diagnosticCollection;

function activate(context) {
    diagnosticCollection = vscode.languages.createDiagnosticCollection('vss');
    context.subscriptions.push(diagnosticCollection);

    // Run diagnostics on open and save
    context.subscriptions.push(
        vscode.workspace.onDidOpenTextDocument(doc => triggerDiagnostics(doc)),
        vscode.workspace.onDidSaveTextDocument(doc => triggerDiagnostics(doc)),
        vscode.workspace.onDidChangeTextDocument(event => triggerDiagnostics(event.document)),
        vscode.workspace.onDidCloseTextDocument(doc => diagnosticCollection.delete(doc.uri))
    );

    // Initial diagnostics for all open VSS files
    vscode.workspace.textDocuments.forEach(doc => triggerDiagnostics(doc));

    // Show welcome message for first activation
    const version = context.extension.packageJSON.version;
    vscode.window.setStatusBarMessage(`VSS Language ${version} activated`, 3000);
}

function findVssExecutable() {
    const platform = process.platform;

    // Check workspace root for bundled vss binary
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (workspaceFolders) {
        const root = workspaceFolders[0].uri.fsPath;
        if (platform === 'win32') {
            const localExe = path.join(root, 'vss', 'vss.exe');
            if (fs.existsSync(localExe)) return `"${localExe}"`;
        } else {
            const localBin = path.join(root, 'vss', 'vss');
            if (fs.existsSync(localBin)) return localBin;
        }
    }

    // Fall back to system PATH
    return platform === 'win32' ? 'vss.exe' : 'vss';
}

let debounceTimers = new Map();

function triggerDiagnostics(document) {
    if (document.languageId !== 'vss') return;

    // Debounce: wait 500ms after last change
    const key = document.uri.toString();
    if (debounceTimers.has(key)) {
        clearTimeout(debounceTimers.get(key));
    }
    debounceTimers.set(key, setTimeout(() => {
        runDiagnostics(document);
        debounceTimers.delete(key);
    }, 500));
}

function runDiagnostics(document) {
    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (!workspaceFolders) return;

    const workspaceRoot = workspaceFolders[0].uri.fsPath;
    const filePath = document.uri.fsPath;
    const vssExe = findVssExecutable();

    const command = `${vssExe} "${filePath}"`;

    exec(command, { cwd: workspaceRoot, timeout: 10000 }, (error, stdout, stderr) => {
        diagnosticCollection.set(document.uri, []);

        const output = (stdout || '') + '\n' + (stderr || '');
        const diagnostics = [];

        // Match VSS 2.1 error format: "error: line N, col M: message"
        // Also handle runtime errors: "runtime error line N: message"
        const errorRegex = /(?:error(?::\s*line\s+(\d+)(?:,\s*col\s+(\d+))?:?\s*(.+)|[^\n]*line\s+(\d+)[,:]?\s*(.+))|runtime error line\s+(\d+):\s*(.+))/gi;

        let match;
        while ((match = errorRegex.exec(output)) !== null) {
            let lineNum, colNum, message;

            if (match[1]) {
                // "error: line N, col M: message"
                lineNum = parseInt(match[1], 10) - 1;
                colNum = match[2] ? parseInt(match[2], 10) - 1 : 0;
                message = match[3] ? match[3].trim() : 'Syntax error';
            } else if (match[4]) {
                // "error line N: message"
                lineNum = parseInt(match[4], 10) - 1;
                colNum = 0;
                message = match[5] ? match[5].trim() : 'Error';
            } else if (match[6]) {
                // "runtime error line N: message"
                lineNum = parseInt(match[6], 10) - 1;
                colNum = 0;
                message = match[7] ? match[7].trim() : 'Runtime error';
            } else {
                continue;
            }

            // Strip ANSI escape codes from message
            message = message.replace(/\x1b\[[0-9;]*m/g, '');

            if (lineNum < 0) lineNum = 0;
            if (colNum < 0) colNum = 0;

            const lineCount = document.lineCount;
            if (lineNum >= lineCount) lineNum = lineCount - 1;

            const line = document.lineAt(lineNum);
            const range = new vscode.Range(
                lineNum, colNum,
                lineNum, line.text.length
            );

            const severity = message.toLowerCase().startsWith('runtime')
                ? vscode.DiagnosticSeverity.Warning
                : vscode.DiagnosticSeverity.Error;

            const diagnostic = new vscode.Diagnostic(range, `VSS: ${message}`, severity);
            diagnostic.source = 'vss';
            diagnostics.push(diagnostic);
        }

        diagnosticCollection.set(document.uri, diagnostics);
    });
}

function deactivate() {
    debounceTimers.forEach(timer => clearTimeout(timer));
    debounceTimers.clear();
}

module.exports = {
    activate,
    deactivate
};
