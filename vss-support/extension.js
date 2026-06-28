const vscode = require('vscode');
const { exec } = require('child_process');
const path = require('path');

let diagnosticCollection;

function activate(context) {
    diagnosticCollection = vscode.languages.createDiagnosticCollection('vss');
    context.subscriptions.push(diagnosticCollection);

    // Run diagnostics on open and save
    context.subscriptions.push(
        vscode.workspace.onDidOpenTextDocument(doc => triggerDiagnostics(doc)),
        vscode.workspace.onDidSaveTextDocument(doc => triggerDiagnostics(doc)),
        vscode.workspace.onDidCloseTextDocument(doc => diagnosticCollection.delete(doc.uri))
    );

    // Initial diagnostics for all open VSS files
    vscode.workspace.textDocuments.forEach(doc => triggerDiagnostics(doc));
}

function triggerDiagnostics(document) {
    if (document.languageId !== 'vss') return;

    const workspaceFolders = vscode.workspace.workspaceFolders;
    if (!workspaceFolders) return;

    const workspaceRoot = workspaceFolders[0].uri.fsPath;
    const relativeDocPath = path.relative(workspaceRoot, document.uri.fsPath);

    // Run WSL VSS compiler to check for syntax/runtime errors
    const wslCommand = `wsl ./vss/vss "${relativeDocPath.replace(/\\/g, '/')}"`;

    exec(wslCommand, { cwd: workspaceRoot }, (error, stdout, stderr) => {
        diagnosticCollection.clear();
        const diagnostics = [];

        // Check stdout and stderr for error messages
        const output = stdout + "\n" + stderr;
        const errorRegex = /(?:error|runtime error) line (\d+), col (\d+): (.*)/i;
        const match = errorRegex.exec(output);

        if (match) {
            const line = parseInt(match[1], 10) - 1; // VS Code lines are 0-indexed
            const col = parseInt(match[2], 10) - 1;   // VS Code cols are 0-indexed
            const message = match[3].trim();

            const range = new vscode.Range(line, col, line, col + 10);
            const diagnostic = new vscode.Diagnostic(range, message, vscode.DiagnosticSeverity.Error);
            diagnostics.push(diagnostic);
            diagnosticCollection.set(document.uri, diagnostics);
        }
    });
}

function deactivate() {}

module.exports = {
    activate,
    deactivate
};
