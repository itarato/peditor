#pragma once

#include <unordered_map>

#include "terminal_util.h"
#include "utility.h"

using namespace std;

static unordered_map<EscapeChar, InputStroke> escapeCharToInputStrokeMap{
    {EscapeChar::Up, InputStroke::Up},
    {EscapeChar::Down, InputStroke::Down},
    {EscapeChar::Left, InputStroke::Left},
    {EscapeChar::Right, InputStroke::Right},
    {EscapeChar::CtrlUp, InputStroke::CtrlUp},
    {EscapeChar::CtrlDown, InputStroke::CtrlDown},
    {EscapeChar::CtrlLeft, InputStroke::CtrlLeft},
    {EscapeChar::CtrlRight, InputStroke::CtrlRight},
    {EscapeChar::CtrlAltLeft, InputStroke::CtrlAltLeft},
    {EscapeChar::CtrlAltRight, InputStroke::CtrlAltRight},
    {EscapeChar::Home, InputStroke::Home},
    {EscapeChar::End, InputStroke::End},
    {EscapeChar::PageUp, InputStroke::PageUp},
    {EscapeChar::PageDown, InputStroke::PageDown},
    {EscapeChar::Delete, InputStroke::Delete},
    {EscapeChar::AltLT, InputStroke::AltLT},
    {EscapeChar::AltGT, InputStroke::AltGT},
    {EscapeChar::AltN, InputStroke::AltN},
    {EscapeChar::Alt0, InputStroke::Alt0},
    {EscapeChar::Alt1, InputStroke::Alt1},
    {EscapeChar::Alt2, InputStroke::Alt2},
    {EscapeChar::Alt3, InputStroke::Alt3},
    {EscapeChar::Alt4, InputStroke::Alt4},
    {EscapeChar::Alt5, InputStroke::Alt5},
    {EscapeChar::Alt6, InputStroke::Alt6},
    {EscapeChar::Alt7, InputStroke::Alt7},
    {EscapeChar::Alt8, InputStroke::Alt8},
    {EscapeChar::Alt9, InputStroke::Alt9},
    {EscapeChar::AltMinus, InputStroke::AltMinus},
    {EscapeChar::AltEqual, InputStroke::AltEqual},
};

struct Config {
  int tabSize{2};
  void setTabSize(int newTabSize) { tabSize = newTabSize; }

  // TODO: This is just a default set. This should be populated from a
  // keymapping file defined by the user.
  unordered_map<InputStroke, TextEditorAction> keyMapping{
      {InputStroke::CtrlQ, TextEditorAction::Quit},
      {InputStroke::CtrlS, TextEditorAction::SaveFile},
      {InputStroke::CtrlW, TextEditorAction::SaveFileAs},
      {InputStroke::CtrlO, TextEditorAction::OpenFile},
      {InputStroke::CtrlP, TextEditorAction::MultiPurposeCommand},
      {InputStroke::CtrlD, TextEditorAction::DeleteLine},
      {InputStroke::CtrlZ, TextEditorAction::Undo},
      {InputStroke::CtrlR, TextEditorAction::Redo},
      {InputStroke::CtrlC, TextEditorAction::Copy},
      {InputStroke::CtrlV, TextEditorAction::Paste},
      {InputStroke::CtrlX, TextEditorAction::SelectionToggle},
      {InputStroke::CtrlN, TextEditorAction::JumpNextSearchHit},
      {InputStroke::CtrlB, TextEditorAction::JumpPrevSearchHit},
      {InputStroke::Backspace, TextEditorAction::Backspace},
      {InputStroke::CtrlBackspace, TextEditorAction::WordBackspace},
      {InputStroke::Enter, TextEditorAction::Enter},
      {InputStroke::Tab, TextEditorAction::Tab},
      {InputStroke::Down, TextEditorAction::CursorDown},
      {InputStroke::Up, TextEditorAction::CursorUp},
      {InputStroke::Left, TextEditorAction::CursorLeft},
      {InputStroke::Right, TextEditorAction::CursorRight},
      {InputStroke::Home, TextEditorAction::CursorHome},
      {InputStroke::End, TextEditorAction::CursorEnd},
      {InputStroke::CtrlUp, TextEditorAction::ScrollUp},
      {InputStroke::CtrlDown, TextEditorAction::ScrollDown},
      {InputStroke::CtrlLeft, TextEditorAction::CursorWordJumpLeft},
      {InputStroke::CtrlRight, TextEditorAction::CursosWordJumpRight},
      {InputStroke::CtrlAltLeft, TextEditorAction::SplitUnitToPrev},
      {InputStroke::CtrlAltRight, TextEditorAction::SplitUnitToNext},
      {InputStroke::PageUp, TextEditorAction::CursorPageUp},
      {InputStroke::PageDown, TextEditorAction::CursorPageDown},
      {InputStroke::Delete, TextEditorAction::InsertDelete},
      {InputStroke::AltLT, TextEditorAction::LineIndentLeft},
      {InputStroke::AltGT, TextEditorAction::LineIndentRight},
      {InputStroke::AltMinus, TextEditorAction::LineMoveBackward},
      {InputStroke::AltEqual, TextEditorAction::LineMoveForward},
      {InputStroke::AltN, TextEditorAction::NewTextView},
      {InputStroke::Alt1, TextEditorAction::ChangeActiveView0},
      {InputStroke::Alt2, TextEditorAction::ChangeActiveView1},
      {InputStroke::Alt3, TextEditorAction::ChangeActiveView2},
      {InputStroke::Alt4, TextEditorAction::ChangeActiveView3},
      {InputStroke::Alt5, TextEditorAction::ChangeActiveView4},
      {InputStroke::Alt6, TextEditorAction::ChangeActiveView5},
      {InputStroke::Alt7, TextEditorAction::ChangeActiveView6},
      {InputStroke::Alt8, TextEditorAction::ChangeActiveView7},
      {InputStroke::Alt9, TextEditorAction::ChangeActiveView8},
      {InputStroke::Alt0, TextEditorAction::ChangeActiveView9},
  };

  TextEditorAction textEditorActionForKeystroke(TypedChar tc) {
    return keyMapping[inputStrokeForKeystroke(tc)];
  }

  InputStroke inputStrokeForKeystroke(TypedChar tc) {
    if (tc.is_escape()) return escapeCharToInputStrokeMap[tc.escape()];

    char simpleChar = tc.simple();

    if (simpleChar == ctrlKey('q')) return InputStroke::CtrlQ;
    if (simpleChar == ctrlKey('s')) return InputStroke::CtrlS;
    if (simpleChar == ctrlKey('w')) return InputStroke::CtrlW;
    if (simpleChar == ctrlKey('o')) return InputStroke::CtrlO;
    if (simpleChar == ctrlKey('p')) return InputStroke::CtrlP;
    if (simpleChar == ctrlKey('d')) return InputStroke::CtrlD;
    if (simpleChar == ctrlKey('z')) return InputStroke::CtrlZ;
    if (simpleChar == ctrlKey('r')) return InputStroke::CtrlR;
    if (simpleChar == ctrlKey('c')) return InputStroke::CtrlC;
    if (simpleChar == ctrlKey('v')) return InputStroke::CtrlV;
    if (simpleChar == ctrlKey('x')) return InputStroke::CtrlX;
    if (simpleChar == ctrlKey('n')) return InputStroke::CtrlN;
    if (simpleChar == ctrlKey('b')) return InputStroke::CtrlB;
    if (simpleChar == BACKSPACE) return InputStroke::Backspace;
    if (simpleChar == CTRL_BACKSPACE) return InputStroke::CtrlBackspace;
    if (simpleChar == ENTER) return InputStroke::Enter;
    if (simpleChar == TAB) return InputStroke::Tab;

    return InputStroke::Generic;
  }
};
