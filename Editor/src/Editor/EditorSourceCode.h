#pragma once

struct EditorState;

// After a project was loaded, this function will determine the git folder location
// Such that it is persisted.
void DetermineProjectSourceCodeGitDirectory(EditorState* editor_state);

void UpdateProjectSourceCodeInfo(EditorState* editor_state);

void TickProjectSourceCodeInfo(EditorState* editor_state);