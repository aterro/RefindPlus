/*
 * BootMaster/menu.c
 * Menu functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Modifications copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */
#include "pointer.h"
#include "global.h"
#include "screenmgt.h"
#include "lib.h"
#include "menu.h"
#include "config.h"
#include "libeg.h"
#include "libegint.h"
#include "line_edit.h"
#include "mystrings.h"
#include "icns.h"
#include "scan.h"
#include "../include/refit_call_wrapper.h"
#include <Library/UefiBootServicesTableLib.h>
#include "../include/egemb_back_selected_small.h"
#include "../include/egemb_back_selected_big.h"
#include "../include/egemb_arrow_left.h"
#include "../include/egemb_arrow_right.h"

// other menu definitions

#define MENU_FUNCTION_INIT            (0)
#define MENU_FUNCTION_CLEANUP         (1)
#define MENU_FUNCTION_PAINT_ALL       (2)
#define MENU_FUNCTION_PAINT_SELECTION (3)
#define MENU_FUNCTION_PAINT_TIMEOUT   (4)
#define MENU_FUNCTION_PAINT_HINTS     (5)

// typedef VOID (*MENU_STYLE_FUNC)(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText);

static CHAR16 ArrowUp[2]   = { ARROW_UP, 0 };
static CHAR16 ArrowDown[2] = { ARROW_DOWN, 0 };
static UINTN  TileSizes[2] = { 144, 64 };

// Text and icon spacing constants.
#define TEXT_YMARGIN       (2)
#define TITLEICON_SPACING (16)

#define TILE_XSPACING      (8)
#define TILE_YSPACING     (16)

// Alignment values for PaintIcon()
#define ALIGN_RIGHT 1
#define ALIGN_LEFT  0

static EG_IMAGE *SelectionImages[2]      = { NULL, NULL };
static EG_PIXEL SelectionBackgroundPixel = { 0xff, 0xff, 0xff, 0 };

EFI_EVENT *WaitList       = NULL;
UINTN      WaitListLength = 0;

// Pointer variables
BOOLEAN PointerEnabled    = FALSE;
BOOLEAN PointerActive     = FALSE;
BOOLEAN DrawSelection     = TRUE;

extern EFI_GUID          RefindPlusGuid;
extern REFIT_MENU_ENTRY  MenuEntryReturn;

static REFIT_MENU_ENTRY  MenuEntryYes  = { L"Yes", TAG_RETURN, 1, 0, 0, NULL, NULL, NULL };
static REFIT_MENU_ENTRY  MenuEntryNo   = { L"No", TAG_RETURN, 1, 0, 0, NULL, NULL, NULL };

//
// Graphics helper functions
//

// Forward declaration for GetMenuItemCenter
static
VOID GetMenuItemCenter (
    IN  REFIT_MENU_SCREEN *Screen,
    IN  SCROLL_STATE      *State,
    IN  UINTN              ItemIndex,
    OUT UINTN             *CenterX,
    OUT UINTN             *CenterY
);
static
VOID InitSelection (VOID) {
    EG_IMAGE  *TempSmallImage    = NULL;
    EG_IMAGE  *TempBigImage      = NULL;
    BOOLEAN    LoadedSmallImage  = FALSE;
    BOOLEAN    TaintFree         = TRUE;

    if (!AllowGraphicsMode || (SelectionImages[0] != NULL)) {
        return;
    }

    // load small selection image
    if (GlobalConfig.SelectionSmallFileName != NULL) {
        TempSmallImage = egLoadImage (SelfDir, GlobalConfig.SelectionSmallFileName, TRUE);
    }

    if (TempSmallImage == NULL) {
        TempSmallImage = egPrepareEmbeddedImage (&egemb_back_selected_small, TRUE);
    }
    else {
        LoadedSmallImage = TRUE;
    }

    if ((TempSmallImage->Width != TileSizes[1]) || (TempSmallImage->Height != TileSizes[1])) {
        SelectionImages[1] = egScaleImage (TempSmallImage, TileSizes[1], TileSizes[1]);
    }
    else {
        SelectionImages[1] = egCopyImage (TempSmallImage);
    }

    // load big selection image
    if (GlobalConfig.SelectionBigFileName != NULL) {
        TempBigImage = egLoadImage (SelfDir, GlobalConfig.SelectionBigFileName, TRUE);
    }

    if (TempBigImage == NULL) {
        if (TempSmallImage->Width > 128 || TempSmallImage->Height > 128) {
            TaintFree = FALSE;
        }

        if (TaintFree && LoadedSmallImage) {
           // calculate big selection image from small one
           TempBigImage = egCopyImage (TempSmallImage);
        }
        else {
           TempBigImage = egPrepareEmbeddedImage (&egemb_back_selected_big, TRUE);
        }
    }

    if ((TempBigImage->Width != TileSizes[0]) || (TempBigImage->Height != TileSizes[0])) {
        SelectionImages[0] = egScaleImage (TempBigImage, TileSizes[0], TileSizes[0]);
    }
    else {
        SelectionImages[0] = egCopyImage (TempBigImage);
    }

    egFreeImage (TempSmallImage);
    egFreeImage (TempBigImage);
} // VOID InitSelection()

//
// Scrolling functions
//

static
VOID InitScroll (
    OUT SCROLL_STATE *State,
    IN UINTN          ItemCount,
    IN UINTN          VisibleSpace
) {
    State->PreviousSelection = State->CurrentSelection = 0;
    State->MaxIndex = (INTN)ItemCount - 1;
    State->FirstVisible = 0;

    if (AllowGraphicsMode) {
        State->MaxVisible = ScreenW / (TileSizes[0] + TILE_XSPACING) - 1;
    }
    else {
        State->MaxVisible = ConHeight - 4;
    }

    if ((VisibleSpace > 0) && (VisibleSpace < State->MaxVisible)) {
        State->MaxVisible = (INTN)VisibleSpace;
    }

    State->PaintAll        = TRUE;
    State->PaintSelection  = FALSE;
    State->LastVisible     = State->FirstVisible + State->MaxVisible - 1;
}

// Adjust variables relating to the scrolling of tags, for when a selected icon isn't
// visible given the current scrolling condition.
static
VOID AdjustScrollState (
    IN SCROLL_STATE *State
) {
    // Scroll forward
    if (State->CurrentSelection > State->LastVisible) {
        State->LastVisible   = State->CurrentSelection;
        State->FirstVisible  = 1 + State->CurrentSelection - State->MaxVisible;

        if (State->FirstVisible < 0) {
            // should not happen, but just in case.
            State->FirstVisible = 0;
        }

        State->PaintAll = TRUE;
    }

    // Scroll backward
    if (State->CurrentSelection < State->FirstVisible) {
        State->FirstVisible  = State->CurrentSelection;
        State->LastVisible   = State->CurrentSelection + State->MaxVisible - 1;
        State->PaintAll      = TRUE;
    }
} // static VOID AdjustScrollState

static
VOID UpdateScroll (
    IN OUT SCROLL_STATE  *State,
    IN UINTN              Movement
) {
    State->PreviousSelection = State->CurrentSelection;

    switch (Movement) {
        case SCROLL_LINE_LEFT:
            if (State->CurrentSelection > 0) {
                State->CurrentSelection --;
            }

            break;

        case SCROLL_LINE_RIGHT:
            if (State->CurrentSelection < State->MaxIndex) {
                State->CurrentSelection ++;
            }

            break;

        case SCROLL_LINE_UP:
            if (State->ScrollMode == SCROLL_MODE_ICONS) {
                if (State->CurrentSelection >= State->InitialRow1) {
                    if (State->MaxIndex > State->InitialRow1) {
                        // avoid division by 0!
                        State->CurrentSelection = State->FirstVisible +
                            (State->LastVisible      - State->FirstVisible) *
                            (State->CurrentSelection - State->InitialRow1) /
                            (State->MaxIndex         - State->InitialRow1);
                    }
                    else {
                        State->CurrentSelection = State->FirstVisible;
                    }
                }
            }
            else {
                if (State->CurrentSelection > 0) {
                    State->CurrentSelection--;
                }
            }

            break;

        case SCROLL_LINE_DOWN:
            if (State->ScrollMode == SCROLL_MODE_ICONS) {
                if (State->CurrentSelection <= State->FinalRow0) {
                    if (State->LastVisible > State->FirstVisible) {
                        // avoid division by 0!
                        State->CurrentSelection = State->InitialRow1 +
                            (State->MaxIndex         - State->InitialRow1) *
                            (State->CurrentSelection - State->FirstVisible) /
                            (State->LastVisible      - State->FirstVisible);
                    }
                    else {
                        State->CurrentSelection = State->InitialRow1;
                    }
                }
            }
            else {
                if (State->CurrentSelection < State->MaxIndex)
                State->CurrentSelection++;
            }

            break;

        case SCROLL_PAGE_UP:
            if (State->CurrentSelection <= State->FinalRow0) {
                State->CurrentSelection -= State->MaxVisible;
            }
            else if (State->CurrentSelection == State->InitialRow1) {
                State->CurrentSelection = State->FinalRow0;
            }
            else {
                State->CurrentSelection = State->InitialRow1;
            }

            if (State->CurrentSelection < 0) {
                State->CurrentSelection = 0;
            }

            break;

        case SCROLL_FIRST:
            if (State->CurrentSelection > 0) {
                State->PaintAll = TRUE;
                State->CurrentSelection = 0;
            }

            break;

        case SCROLL_PAGE_DOWN:
            if (State->CurrentSelection  < State->FinalRow0) {
                State->CurrentSelection += State->MaxVisible;
                if (State->CurrentSelection > State->FinalRow0) {
                    State->CurrentSelection = State->FinalRow0;
                }
            }
            else if (State->CurrentSelection == State->FinalRow0) {
                State->CurrentSelection++;
            }
            else {
                State->CurrentSelection = State->MaxIndex;
            }

            if (State->CurrentSelection > State->MaxIndex) {
                State->CurrentSelection = State->MaxIndex;
            }

            break;

        case SCROLL_LAST:
            if (State->CurrentSelection < State->MaxIndex) {
                State->PaintAll = TRUE;
                State->CurrentSelection = State->MaxIndex;
            }

            break;

        case SCROLL_NONE:

            break;
    } // switch

    if (State->ScrollMode == SCROLL_MODE_TEXT) {
        AdjustScrollState (State);
    }

    if (!State->PaintAll && State->CurrentSelection != State->PreviousSelection) {
        State->PaintSelection = TRUE;
    }
    State->LastVisible = State->FirstVisible + State->MaxVisible - 1;
} // static VOID UpdateScroll()


//
// menu helper functions
//

CHAR16 * MenuExitInfo (
    IN UINTN MenuExit
) {
    CHAR16 *MenuExitData = NULL;

    switch (MenuExit) {
        case 1:  MenuExitData = L"ENTER";   break;
        case 2:  MenuExitData = L"ESCAPE";  break;
        case 3:  MenuExitData = L"DETAILS"; break;
        case 4:  MenuExitData = L"TIMEOUT"; break;
        case 5:  MenuExitData = L"EJECT";   break;
        case 6:  MenuExitData = L"HIDE";    break;
        default: MenuExitData = L"RETURN";  // Actually '99'
    } // switch

    return MenuExitData;
} // CHAR16 * MenuExitInfo()

VOID AddMenuInfoLine (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16            *InfoLine
) {
    #if REFIT_DEBUG > 0
    LOG(4, LOG_LINE_NORMAL, L"Adding Menu Info Line:- '%s'", InfoLine);
    #endif

    AddListElement ((VOID ***) &(Screen->InfoLines), &(Screen->InfoLineCount), InfoLine);
} // VOID AddMenuInfoLine()

VOID AddMenuEntry (
    IN REFIT_MENU_SCREEN *Screen,
    IN REFIT_MENU_ENTRY  *Entry
) {
    #if REFIT_DEBUG > 0
    LOG(4, LOG_LINE_NORMAL, L"Adding Menu Entry to %s:- '%s'", Screen->Title, Entry->Title);
    #endif

    AddListElement ((VOID ***) &(Screen->Entries), &(Screen->EntryCount), Entry);
} // VOID AddMenuEntry()

static
INTN FindMenuShortcutEntry (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16            *Defaults
) {
    UINTN   i, j = 0, ShortcutLength;
    CHAR16 *Shortcut;

    while ((Shortcut = FindCommaDelimited (Defaults, j)) != NULL) {
        ShortcutLength = StrLen (Shortcut);
        if (ShortcutLength == 1) {
            if (Shortcut[0] >= 'a' && Shortcut[0] <= 'z') {
                Shortcut[0] -= ('a' - 'A');
            }

            if (Shortcut[0]) {
                for (i = 0; i < Screen->EntryCount; i++) {
                    if (Screen->Entries[i]->ShortcutDigit  == Shortcut[0] ||
                        Screen->Entries[i]->ShortcutLetter == Shortcut[0]
                    ) {
                        MyFreePool (&Shortcut);

                        return i;
                    }
                } // for
            }
        }
        else if (ShortcutLength > 1) {
            for (i = 0; i < Screen->EntryCount; i++) {
                if (StriSubCmp (Shortcut, Screen->Entries[i]->Title)) {
                    MyFreePool (&Shortcut);

                    return i;
                }
            } // for
        }

        MyFreePool (&Shortcut);
        j++;
    } // while

    return -1;
} // static INTN FindMenuShortcutEntry()

// Identify the end of row 0 and the beginning of row 1; store the results in the
// appropriate fields in State. Also reduce MaxVisible if that value is greater
// than the total number of row-0 tags and if we are in an icon-based screen
static
VOID IdentifyRows (
    IN SCROLL_STATE      *State,
    IN REFIT_MENU_SCREEN *Screen
) {
    UINTN i;

    State->FinalRow0 = 0;
    State->InitialRow1 = State->MaxIndex;
    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0) {
            State->FinalRow0 = i;
        }
        else if ((Screen->Entries[i]->Row == 1) && (State->InitialRow1 > i)) {
            State->InitialRow1 = i;
        }
    } // for

    if ((State->ScrollMode == SCROLL_MODE_ICONS) &&
        (State->MaxVisible > (State->FinalRow0 + 1))
    ) {
        State->MaxVisible = State->FinalRow0 + 1;
    }
} // static VOID IdentifyRows()

//
// generic menu function
//
static UINT64 GetCurrentMS(VOID)
{
    EFI_TIME Time;
    EFI_STATUS Status = REFIT_CALL_2_WRAPPER(gRT->GetTime, &Time, NULL);
    if (EFI_ERROR(Status)) {
        return 0;
    }
    return ((Time.Hour * 3600) + (Time.Minute * 60) + Time.Second) * 1000 + (Time.Nanosecond / 1000000);
}



static VOID SaveScreen(VOID) {
    EG_PIXEL Black = { 0x0, 0x0, 0x0, 0 };
    EFI_INPUT_KEY key;
    EFI_STATUS PointerStatus;

    egClearScreen(&Black);

    while(TRUE) {
        PointerStatus = pdUpdateState();
        if (!EFI_ERROR(PointerStatus)) {
            if (pdGetState().Press) {
                break;
            }
        }
        if (REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key) == EFI_SUCCESS) {
            break;
        }
        REFIT_CALL_1_WRAPPER(gBS->Stall, 100000);
    }

    if (AllowGraphicsMode)
        SwitchToGraphicsAndClear(TRUE);
    ReadAllKeyStrokes();
}

UINTN RunGenericMenu(IN REFIT_MENU_SCREEN *Screen, IN MENU_STYLE_FUNC StyleFunc, IN OUT INTN *DefaultEntryIndex, OUT REFIT_MENU_ENTRY **ChosenEntry)
{
    SCROLL_STATE State;
    EFI_STATUS Status;
    EFI_INPUT_KEY key;
    INTN ShortcutEntry;
    BOOLEAN HaveTimeout;
    INTN PreviousTime = -1;
    CHAR16 TimeoutMessage[256];
    CHAR16 KeyAsString[2];
    UINTN MenuExit;
    UINTN Item;
    UINT64 CurrentTimeMs = 0;
    UINT64 LastInputMs = 0;
    UINTN ScreensaverTimeoutMs = 0;
    UINTN MenuTimeoutMs = 0;
    BOOLEAN InputDetectedThisIteration = FALSE;
    EFI_STATUS PointerStatusLocal;
    POINTER_STATE CurrentPointerState = {0};
    POINTER_STATE PreviousPointerStateInMenu = {0};
    BOOLEAN ClickDetected = FALSE;
    static BOOLEAN pointerShouldBeVisible = FALSE;
    BOOLEAN TimerPermanentlyDisabled = FALSE;

    if (Screen->TimeoutSeconds > 0) {
        HaveTimeout = TRUE;
        MenuTimeoutMs = Screen->TimeoutSeconds * 1000;
    } else {
        HaveTimeout = FALSE;
        MenuTimeoutMs = 0;
    }
    MenuExit = MENU_EXIT_ZERO;

    StyleFunc(Screen, &State, MENU_FUNCTION_INIT, NULL);
    IdentifyRows(&State, Screen);
    if (*DefaultEntryIndex >= 0 && *DefaultEntryIndex <= State.MaxIndex) {
        State.CurrentSelection = *DefaultEntryIndex;
        UpdateScroll(&State, SCROLL_NONE);
        // Position pointer at center of default selection
        if (PointerEnabled) {
            UINTN PointerX, PointerY;
            GetMenuItemCenter (Screen, &State, State.CurrentSelection, &PointerX, &PointerY);
            pdSetPosition (PointerX, PointerY);
        }
    }

    if (Screen->TimeoutSeconds == -1) {
        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
        if (Status == EFI_SUCCESS) {
            KeyAsString[0] = key.UnicodeChar;
            KeyAsString[1] = 0;
            ShortcutEntry = FindMenuShortcutEntry(Screen, KeyAsString);
            if (ShortcutEntry >= 0) {
                State.CurrentSelection = ShortcutEntry;
                MenuExit = MENU_EXIT_ENTER;
            }
            REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, FALSE);
            if (HaveTimeout) {
                TimerPermanentlyDisabled = TRUE;
                StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, L"");
            }
        }
    }

    State.PaintAll = TRUE;

    CurrentTimeMs = GetCurrentMS();
    LastInputMs = CurrentTimeMs;
    ScreensaverTimeoutMs = GlobalConfig.ScreensaverTime * 1000;

    while (MenuExit == MENU_EXIT_ZERO) {
        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
        if (Status == EFI_SUCCESS) {
            InputDetectedThisIteration = TRUE;
        } else {
            InputDetectedThisIteration = FALSE;
        }

        if (PointerEnabled) {
            PointerStatusLocal = pdUpdateState();
            if (!EFI_ERROR(PointerStatusLocal)) {
                PointerActive = TRUE;
                CurrentPointerState = pdGetState();
                if (CurrentPointerState.X != PreviousPointerStateInMenu.X ||
                    CurrentPointerState.Y != PreviousPointerStateInMenu.Y ||
                    CurrentPointerState.Press != PreviousPointerStateInMenu.Press) {
                    InputDetectedThisIteration = TRUE;
                }
            } else {
                PointerActive = FALSE;
            }
        }

        CurrentTimeMs = GetCurrentMS();

        if (InputDetectedThisIteration) {
            LastInputMs = CurrentTimeMs;

            if (HaveTimeout) {
                TimerPermanentlyDisabled = TRUE;
                StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, L"");
            }
        } else {
            if (!TimerPermanentlyDisabled) {
                UINT64 ElapsedSinceLastInputMs = CurrentTimeMs - LastInputMs;

                if (HaveTimeout && MenuTimeoutMs > 0) {
                    if (ElapsedSinceLastInputMs >= MenuTimeoutMs) {
                        MenuExit = MENU_EXIT_TIMEOUT;
                    } else {
                        INTN remainingSeconds = (INTN)((MenuTimeoutMs - ElapsedSinceLastInputMs + 999) / 1000);
                        if (remainingSeconds < 0) remainingSeconds = 0;

                        if (remainingSeconds != PreviousTime) {
                            SPrint(TimeoutMessage, 255, L"%s in %d seconds", Screen->TimeoutText, remainingSeconds);
                            StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, TimeoutMessage);
                            PreviousTime = remainingSeconds;
                        }
                    }
                }

                if (ScreensaverTimeoutMs > 0) {
                    if (ElapsedSinceLastInputMs >= ScreensaverTimeoutMs) {
                        SaveScreen();
                        State.PaintAll = TRUE;
                        LastInputMs = CurrentTimeMs;
                    }
                }
            }
        }

        if (State.PaintAll) {
            if (PointerEnabled && pointerShouldBeVisible) { pdClear(); }
            StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_ALL, NULL);
            State.PaintAll = FALSE;
            State.PaintSelection = FALSE;
        } else if (State.PaintSelection) {
            if (PointerEnabled && pointerShouldBeVisible) { pdClear(); }
            gSuppressPointerDraw = TRUE;
            StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_SELECTION, NULL);
            State.PaintSelection = FALSE;
            gSuppressPointerDraw = FALSE;
        }

        if (PointerEnabled && pointerShouldBeVisible && !gSuppressPointerDraw) {
            pdDraw();
        }

        if (MenuExit != MENU_EXIT_ZERO) {
            break;
        }

        if (Status == EFI_SUCCESS) {
            if (PointerEnabled) {
                pdClear();
            }
            pointerShouldBeVisible = FALSE;
            DrawSelection = TRUE;
            switch (key.ScanCode) {
                case SCAN_UP: UpdateScroll(&State, SCROLL_LINE_UP); State.PaintSelection = TRUE; break;
                case SCAN_LEFT: UpdateScroll(&State, SCROLL_LINE_LEFT); State.PaintSelection = TRUE; break;
                case SCAN_DOWN: UpdateScroll(&State, SCROLL_LINE_DOWN); State.PaintSelection = TRUE; break;
                case SCAN_RIGHT: UpdateScroll(&State, SCROLL_LINE_RIGHT); State.PaintSelection = TRUE; break;
                case SCAN_HOME: UpdateScroll(&State, SCROLL_FIRST); State.PaintSelection = TRUE; break;
                case SCAN_END: UpdateScroll(&State, SCROLL_LAST); State.PaintSelection = TRUE; break;
                case SCAN_PAGE_UP: UpdateScroll(&State, SCROLL_PAGE_UP); State.PaintSelection = TRUE; break;
                case SCAN_PAGE_DOWN: UpdateScroll(&State, SCROLL_PAGE_DOWN); State.PaintSelection = TRUE; break;
                case SCAN_ESC: MenuExit = MENU_EXIT_ESCAPE; break;
                case SCAN_INSERT: case SCAN_F2: MenuExit = MENU_EXIT_DETAILS; break;
                case SCAN_DELETE: MenuExit = MENU_EXIT_HIDE; break;
                case SCAN_F10: egScreenShot(); State.PaintAll = TRUE; break;
                case 0x0016: if (EjectMedia()) MenuExit = MENU_EXIT_ESCAPE; break;
                default:
                    if (key.UnicodeChar == L'\r' || key.UnicodeChar == L'\n') {
                        MenuExit = MENU_EXIT_ENTER;
                        break;
                    }
                    KeyAsString[0] = key.UnicodeChar;
                    KeyAsString[1] = 0;
                    ShortcutEntry = FindMenuShortcutEntry(Screen, KeyAsString);
                    if (ShortcutEntry >= 0) {
                        State.CurrentSelection = ShortcutEntry;
                        MenuExit = MENU_EXIT_ENTER;
                    }
                    break;
            }
        } else if (PointerActive) {
            pointerShouldBeVisible = TRUE;
            ClickDetected = CurrentPointerState.Press;
            gSuppressPointerDraw = FALSE;
             if (StyleFunc != MainMenuStyle) {
                if (ClickDetected) { gSuppressPointerDraw = FALSE; MenuExit = MENU_EXIT_ENTER;}
            } else {
                State.PreviousSelection = State.CurrentSelection;
                Item = FindMainMenuItem(Screen, &State, CurrentPointerState.X, CurrentPointerState.Y);
                switch (Item) {
                    case POINTER_NO_ITEM:
                        if(DrawSelection) { 
                        DrawSelection = FALSE;
                        State.PaintSelection = FALSE;
                        State.PaintAll = TRUE;
                        if (ClickDetected || CurrentPointerState.Press) {
                        gSuppressPointerDraw = FALSE;
                        pdDraw();
                        MenuExit = MENU_EXIT_ZERO;
                        }
                        } 
                        break;
                    case POINTER_LEFT_ARROW:
                        if (ClickDetected) {
                            UpdateScroll(&State, SCROLL_PAGE_UP);
                            State.PaintAll = TRUE;
                        }
                        DrawSelection = FALSE;
                        break;
                    case POINTER_RIGHT_ARROW:
                        if (ClickDetected) {
                            UpdateScroll(&State, SCROLL_PAGE_DOWN);
                            State.PaintAll = TRUE;
                        }
                        DrawSelection = FALSE;
                        break;
                    default:
                        if (!DrawSelection || Item != State.CurrentSelection) {
                            State.CurrentSelection = Item;
                            State.PaintSelection = TRUE;
                        }
                        DrawSelection = TRUE;
                        if (ClickDetected) {
                            MenuExit = MENU_EXIT_ENTER;
                        }
                        break;
                }
            }
            PreviousPointerStateInMenu = CurrentPointerState;
        }

        REFIT_CALL_1_WRAPPER(gBS->Stall, 1000);
    } 

    pointerShouldBeVisible = FALSE;
    if (PointerEnabled) {
        pdClear();
    }
    StyleFunc(Screen, &State, MENU_FUNCTION_CLEANUP, NULL);
    if (ChosenEntry) {
        *ChosenEntry = Screen->Entries[State.CurrentSelection];
    }
    *DefaultEntryIndex = State.CurrentSelection;
    return MenuExit;
} // UINTN RunGenericMenu()

//
// text-mode generic style
//

// Show information lines in text mode.
static
VOID ShowTextInfoLines (
    IN REFIT_MENU_SCREEN *Screen
) {
    INTN i;

    BeginTextScreen (Screen->Title);
    if (Screen->InfoLineCount > 0) {
        REFIT_CALL_2_WRAPPER(
            gST->ConOut->SetAttribute,
            gST->ConOut,
            ATTR_BASIC
        );

        for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
            REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 3, 4 + i);
            REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, Screen->InfoLines[i]);
        }
    }
} // VOID ShowTextInfoLines()

// Do most of the work for text-based menus.
VOID TextMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              Function,
    IN CHAR16            *ParamText
) {
    INTN  i;
    UINTN MenuWidth;
    UINTN ItemWidth;
    UINTN MenuHeight;

    static UINTN    MenuPosY;
    static CHAR16 **DisplayStrings;

    State->ScrollMode = SCROLL_MODE_TEXT;

    switch (Function) {
        case MENU_FUNCTION_INIT:
            // vertical layout
            MenuPosY = 4;
            if (Screen->InfoLineCount > 0) {
                MenuPosY += Screen->InfoLineCount + 1;
            }

            MenuHeight = ConHeight - MenuPosY - 3;
            if (Screen->TimeoutSeconds > 0) {
                MenuHeight -= 2;
            }
            InitScroll (State, Screen->EntryCount, MenuHeight);

            // determine width of the menu
            MenuWidth = 20;  // minimum

            for (i = 0; i <= State->MaxIndex; i++) {
                ItemWidth = StrLen (Screen->Entries[i]->Title);
                if (MenuWidth < ItemWidth) {
                    MenuWidth = ItemWidth;
                }
            }

            MenuWidth += 2;
            if (MenuWidth > ConWidth - 3) {
                MenuWidth = ConWidth - 3;
            }

            // prepare strings for display
            DisplayStrings = AllocatePool (sizeof (CHAR16 *) * Screen->EntryCount);
            for (i = 0; i <= State->MaxIndex; i++) {
                // Note: Theoretically, SPrint() is a cleaner way to do this; but the
                // description of the StrSize parameter to SPrint implies it is measured
                // in characters, but in practice both TianoCore and GNU-EFI seem to
                // use bytes instead, resulting in truncated displays. I could just
                // double the size of the StrSize parameter, but that seems unsafe in
                // case a future library change starts treating this as characters, so
                // I'm doing it the hard way in this instance.
                // TODO: Review the above and possibly change other uses of SPrint()
                DisplayStrings[i] = AllocateZeroPool (2 * sizeof (CHAR16));
                DisplayStrings[i][0] = L' ';
                MergeStrings (&DisplayStrings[i], Screen->Entries[i]->Title, 0);
                if (StrLen (DisplayStrings[i]) > MenuWidth) {
                    DisplayStrings[i][MenuWidth - 1] = 0;
                }
                // TODO: use more elaborate techniques for shortening too long strings (ellipses in the middle)
                // TODO: account for double-width characters
            } // for

            break;

        case MENU_FUNCTION_CLEANUP:
            // release temporary memory
            for (i = 0; i <= State->MaxIndex; i++) {
                MyFreePool (&DisplayStrings[i]);
            }
            MyFreePool (&DisplayStrings);

            break;

        case MENU_FUNCTION_PAINT_ALL:
            // paint the whole screen (initially and after scrolling)

            ShowTextInfoLines (Screen);
            for (i = 0; i <= State->MaxIndex; i++) {
                if (i >= State->FirstVisible && i <= State->LastVisible) {
                    REFIT_CALL_3_WRAPPER(
                        gST->ConOut->SetCursorPosition,
                        gST->ConOut,
                        2,
                        MenuPosY + (i - State->FirstVisible)
                    );

                    if (i == State->CurrentSelection) {
                        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_CHOICE_CURRENT);
                    }
                    else if (DisplayStrings[i]) {
                        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_CHOICE_BASIC);
                        REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, DisplayStrings[i]);
                    }
                }
            }

            // scrolling indicators
            REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_SCROLLARROW);
            REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, MenuPosY);

            if (State->FirstVisible > 0) {
                gST->ConOut->OutputString (gST->ConOut, ArrowUp);
            }
            else {
                gST->ConOut->OutputString (gST->ConOut, L" ");
            }

            gST->ConOut->SetCursorPosition (gST->ConOut, 0, MenuPosY + State->MaxVisible);

            if (State->LastVisible < State->MaxIndex) {
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, ArrowDown);
            }
            else {
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, L" ");
            }

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
               if (Screen->Hint1 != NULL) {
                   REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 2);
                   REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, Screen->Hint1);
               }

               if (Screen->Hint2 != NULL) {
                   REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 1);
                   REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, Screen->Hint2);
               }
            }

            break;

        case MENU_FUNCTION_PAINT_SELECTION:
            // redraw selection cursor
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition,
                gST->ConOut, 2,
                MenuPosY + (State->PreviousSelection - State->FirstVisible)
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut,
                ATTR_CHOICE_BASIC
            );
            if (DisplayStrings[State->PreviousSelection] != NULL) {
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->OutputString,
                    gST->ConOut,
                    DisplayStrings[State->PreviousSelection]
                );
            }
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition,
                gST->ConOut, 2,
                MenuPosY + (State->CurrentSelection - State->FirstVisible)
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut,
                ATTR_CHOICE_CURRENT
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->OutputString,
                gST->ConOut,
                DisplayStrings[State->CurrentSelection]
            );

            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (ParamText[0] == 0) {
                // clear message
                REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_BASIC);
                REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 3);
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, BlankLine + 1);
            }
            else {
                // paint or update message
                REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_ERROR);
                REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 3, ConHeight - 3);
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, ParamText);
            }

            break;
    }
}

//
// graphical generic style
//

inline static
UINTN TextLineHeight (VOID) {
    return egGetFontHeight() + TEXT_YMARGIN * 2;
} // UINTN TextLineHeight()

//
// Display a submenu
//

// Display text with a solid background (MenuBackgroundPixel or SelectionBackgroundPixel).
// Indents text by one character and placed TEXT_YMARGIN pixels down from the
// specified XPos and YPos locations.
static
VOID DrawText (
    IN CHAR16  *Text,
    IN BOOLEAN  Selected,
    IN UINTN    FieldWidth,
    IN UINTN    XPos,
    IN UINTN    YPos
) {
    EG_IMAGE *TextBuffer;
    EG_PIXEL  Bg;

    TextBuffer = egCreateFilledImage (
        FieldWidth,
        TextLineHeight(),
        FALSE,
        &MenuBackgroundPixel
    );

    if (TextBuffer) {
        Bg = MenuBackgroundPixel;
        if (Selected) {
            // draw selection bar background
            egFillImageArea (
                TextBuffer,
                0, 0,
                FieldWidth,
                TextBuffer->Height,
                &SelectionBackgroundPixel
            );
            Bg = SelectionBackgroundPixel;
        }

        // render the text
        egRenderText (
            Text,
            TextBuffer,
            egGetFontCellWidth(),
            TEXT_YMARGIN,
            (Bg.r + Bg.g + Bg.b) / 3
        );

        egDrawImageWithTransparency (
            TextBuffer, NULL,
            XPos, YPos,
            TextBuffer->Width,
            TextBuffer->Height
        );

        egFreeImage (TextBuffer);
    }
} // VOID DrawText()

// Finds the average brightness of the input Image.
// NOTE: Passing an Image that covers the whole screen can strain the
// capacity of a UINTN on a 32-bit system with a very large display.
// Using UINT64 instead is unworkable, since the code won't compile
// on a 32-bit system. As the intended use for this function is to handle
// a single text string's background, this shouldn't be a problem, but it
// may need addressing if it is applied more broadly.
static
UINT8 AverageBrightness (
    EG_IMAGE *Image
) {
    UINTN i;
    UINTN Sum = 0;

    if ((Image != NULL) && ((Image->Width * Image->Height) != 0)) {
        for (i = 0; i < (Image->Width * Image->Height); i++) {
            Sum += (Image->PixelData[i].r + Image->PixelData[i].g + Image->PixelData[i].b);
        }
        Sum /= (Image->Width * Image->Height * 3);
    }

    return (UINT8) Sum;
} // UINT8 AverageBrightness()

// Display text against the screen's background image. Special case: If Text is NULL
// or 0-length, clear the line. Does NOT indent the text or reposition it relative
// to the specified XPos and YPos values.
static
VOID DrawTextWithTransparency (
    IN CHAR16 *Text,
    IN UINTN   XPos,
    IN UINTN   YPos
) {
    UINTN TextWidth;
    EG_IMAGE *TextBuffer = NULL;

    if (Text == NULL) {
        Text = L"";
    }

    egMeasureText (Text, &TextWidth, NULL);

    if (TextWidth == 0) {
       TextWidth = ScreenW;
       XPos      = 0;
    }

    TextBuffer = egCropImage (
        GlobalConfig.ScreenBackground,
        XPos, YPos,
        TextWidth,
        TextLineHeight()
    );

    if (TextBuffer == NULL) {
        return;
    }

    // render the text
    egRenderText (
        Text,
        TextBuffer,
        0, 0,
        AverageBrightness (TextBuffer)
    );

    egDrawImageWithTransparency (
        TextBuffer, NULL,
        XPos, YPos,
        TextBuffer->Width,
        TextBuffer->Height
    );

    egFreeImage (TextBuffer);
}

// Compute the size & position of the window that will hold a subscreen's information.
static
VOID ComputeSubScreenWindowSize (
    REFIT_MENU_SCREEN *Screen,
    SCROLL_STATE      *State,
    UINTN             *XPos,
    UINTN             *YPos,
    UINTN             *Width,
    UINTN             *Height,
    UINTN             *LineWidth
) {
    UINTN i, ItemWidth, HintTop, BannerBottomEdge, TitleWidth;
    UINTN FontCellWidth  = egGetFontCellWidth();
    UINTN FontCellHeight = egGetFontHeight();

    *Width     = 20;
    *Height    = 5;
    TitleWidth = egComputeTextWidth (Screen->Title);

    for (i = 0; i < Screen->InfoLineCount; i++) {
        ItemWidth = StrLen (Screen->InfoLines[i]);

        if (*Width < ItemWidth) {
            *Width = ItemWidth;
        }

        (*Height)++;
    }

    for (i = 0; i <= State->MaxIndex; i++) {
        ItemWidth = StrLen (Screen->Entries[i]->Title);

        if (*Width < ItemWidth) {
            *Width = ItemWidth;
        }

        (*Height)++;
    }

    *Width = (*Width + 2) * FontCellWidth;
    *LineWidth = *Width;

    if (Screen->TitleImage) {
        *Width += (Screen->TitleImage->Width + TITLEICON_SPACING * 2 + FontCellWidth);
    }
    else {
        *Width += FontCellWidth;
    }

    if (*Width < TitleWidth) {
        *Width = TitleWidth + 2 * FontCellWidth;
    }

    // Keep it within the bounds of the screen, or 2/3 of the screen's width
    // for screens over 800 pixels wide
    if (*Width > ScreenW) {
        *Width = ScreenW;
    }

    *XPos = (ScreenW - *Width) / 2;

    // top of hint text
    HintTop  = ScreenH - (FontCellHeight * 3);
    *Height *= TextLineHeight();

    if (Screen->TitleImage &&
        (*Height < (Screen->TitleImage->Height + TextLineHeight() * 4))
    ) {
        *Height = Screen->TitleImage->Height + TextLineHeight() * 4;
    }

    if (GlobalConfig.BannerBottomEdge >= HintTop) {
        // probably a full-screen image; treat it as an empty banner
        BannerBottomEdge = 0;
    }
    else {
        BannerBottomEdge = GlobalConfig.BannerBottomEdge;
    }

    if (*Height > (HintTop - BannerBottomEdge - FontCellHeight * 2)) {
        BannerBottomEdge = 0;
    }

    if (*Height > (HintTop - BannerBottomEdge - FontCellHeight * 2)) {
        // TODO: Implement scrolling in text screen.
        *Height = (HintTop - BannerBottomEdge - FontCellHeight * 2);
    }

    *YPos = ((ScreenH - *Height) / 2);
    if (*YPos < BannerBottomEdge) {
        *YPos = BannerBottomEdge +
            FontCellHeight +
            (HintTop - BannerBottomEdge - *Height) / 2;
    }
} // VOID ComputeSubScreenWindowSize()

// Displays sub-menus
VOID GraphicsMenuStyle (
    IN REFIT_MENU_SCREEN  *Screen,
    IN SCROLL_STATE       *State,
    IN UINTN               Function,
    IN CHAR16             *ParamText
) {
           INTN      i;
           UINTN     ItemWidth;
    static UINTN     LineWidth, MenuWidth, MenuHeight;
    static UINTN     EntriesPosX, EntriesPosY;
    static UINTN     TitlePosX, TimeoutPosY, CharWidth;
           EG_IMAGE *Window;
           EG_PIXEL *BackgroundPixel = &(GlobalConfig.ScreenBackground->PixelData[0]);

    CharWidth = egGetFontCellWidth();
    State->ScrollMode = SCROLL_MODE_TEXT;

    switch (Function) {
        case MENU_FUNCTION_INIT:
            InitScroll (State, Screen->EntryCount, 0);
            ComputeSubScreenWindowSize (
                Screen, State,
                &EntriesPosX, &EntriesPosY,
                &MenuWidth, &MenuHeight,
                &LineWidth
            );

            TimeoutPosY = EntriesPosY + (Screen->EntryCount + 1) * TextLineHeight();

            // initial painting
            SwitchToGraphicsAndClear (TRUE);

            Window = egCreateFilledImage (MenuWidth, MenuHeight, FALSE, BackgroundPixel);

            if (Window) {
                egDrawImage (Window, EntriesPosX, EntriesPosY);
                egFreeImage (Window);
            }

            ItemWidth = egComputeTextWidth (Screen->Title);

            if (MenuWidth > ItemWidth) {
                TitlePosX = EntriesPosX + (MenuWidth - ItemWidth) / 2 - CharWidth;
            }
            else {
               TitlePosX = EntriesPosX;
               if (CharWidth > 0) {
                  i = MenuWidth / CharWidth - 2;
                  if (i > 0) {
                      Screen->Title[i] = 0;
                  }
               }
            }

            break;

        case MENU_FUNCTION_CLEANUP:
            // nothing to do
            break;

        case MENU_FUNCTION_PAINT_ALL:
            ComputeSubScreenWindowSize (
                Screen, State,
                &EntriesPosX, &EntriesPosY,
                &MenuWidth, &MenuHeight,
                &LineWidth
            );

            DrawText (
                Screen->Title,
                FALSE,
                (StrLen (Screen->Title) + 2) * CharWidth,
                TitlePosX,
                EntriesPosY += TextLineHeight()
            );

            if (Screen->TitleImage) {
                BltImageAlpha (
                    Screen->TitleImage,
                    EntriesPosX + TITLEICON_SPACING,
                    EntriesPosY + TextLineHeight() * 2,
                    BackgroundPixel
                );
                EntriesPosX += (Screen->TitleImage->Width + TITLEICON_SPACING * 2);
            }

            EntriesPosY += (TextLineHeight() * 2);

            if (Screen->InfoLineCount > 0) {
                for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
                    DrawText (
                        Screen->InfoLines[i],
                        FALSE, LineWidth,
                        EntriesPosX, EntriesPosY
                    );

                    EntriesPosY += TextLineHeight();
                }

                // also add a blank line
                EntriesPosY += TextLineHeight();
            }

            for (i = 0; i <= State->MaxIndex; i++) {
                DrawText (
                    Screen->Entries[i]->Title,
                    (i == State->CurrentSelection),
                    LineWidth,
                    EntriesPosX,
                    EntriesPosY + i * TextLineHeight()
                );
            }

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
                if ((Screen->Hint1 != NULL) && (StrLen (Screen->Hint1) > 0)) {
                    DrawTextWithTransparency (
                        Screen->Hint1,
                        (ScreenW - egComputeTextWidth (Screen->Hint1)) / 2,
                        ScreenH - (egGetFontHeight() * 3)
                    );
                }

                if ((Screen->Hint2 != NULL) && (StrLen (Screen->Hint2) > 0)) {
                    DrawTextWithTransparency (
                        Screen->Hint2,
                        (ScreenW - egComputeTextWidth (Screen->Hint2)) / 2,
                        ScreenH - (egGetFontHeight() * 2)
                    );
                }
            }

            break;

        case MENU_FUNCTION_PAINT_SELECTION:
            // redraw selection cursor
            DrawText (
                Screen->Entries[State->PreviousSelection]->Title,
                FALSE, LineWidth,
                EntriesPosX,
                EntriesPosY + State->PreviousSelection * TextLineHeight()
            );

            DrawText (
                Screen->Entries[State->CurrentSelection]->Title,
                TRUE, LineWidth,
                EntriesPosX,
                EntriesPosY + State->CurrentSelection * TextLineHeight()
            );

            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            DrawText (ParamText, FALSE, LineWidth, EntriesPosX, TimeoutPosY);

            break;
    }
} // static VOID GraphicsMenuStyle()

//
// graphical main menu style
//

static
VOID DrawMainMenuEntry (
    REFIT_MENU_ENTRY *Entry,
    BOOLEAN           selected,
    UINTN             XPos,
    UINTN             YPos
) {
    EG_IMAGE *Background;

    // if using pointer ... do not draw selection image when not hovering
    if (!selected || !DrawSelection) {
        // Image not selected ... copy background
        egDrawImageWithTransparency (
            Entry->Image,
            Entry->BadgeImage,
            XPos, YPos,
            SelectionImages[Entry->Row]->Width,
            SelectionImages[Entry->Row]->Height
        );
    }
    else {
        Background = egCropImage (
            GlobalConfig.ScreenBackground,
            XPos, YPos,
            SelectionImages[Entry->Row]->Width,
            SelectionImages[Entry->Row]->Height
        );

        if (Background) {
            egComposeImage (
                Background,
                SelectionImages[Entry->Row],
                0, 0
            );

            BltImageCompositeBadge (
                Background,
                Entry->Image,
                Entry->BadgeImage,
                XPos, YPos
            );

            egFreeImage (Background);
        }
    } // if/else !selected
} // VOID DrawMainMenuEntry()

static
VOID PaintAll (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    UINTN                *itemPosX,
    UINTN                 row0PosY,
    UINTN                 row1PosY,
    UINTN                 textPosY
) {
    INTN i;

    if (Screen->Entries[State->CurrentSelection]->Row == 0) {
        AdjustScrollState (State);
    }

    for (i = State->FirstVisible; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0) {
            if (i <= State->LastVisible) {
                DrawMainMenuEntry (
                    Screen->Entries[i],
                    (i == State->CurrentSelection) ? TRUE : FALSE,
                    itemPosX[i - State->FirstVisible],
                    row0PosY
                );
            }
        }
        else {
            DrawMainMenuEntry (
                Screen->Entries[i],
                (i == State->CurrentSelection) ? TRUE : FALSE,
                itemPosX[i],
                row1PosY
            );
        }
    }

    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL) &&
        (!PointerActive || (PointerActive && DrawSelection))
    ) {
        DrawTextWithTransparency (L"", 0, textPosY);
        DrawTextWithTransparency (
            Screen->Entries[State->CurrentSelection]->Title,
            (ScreenW - egComputeTextWidth (Screen->Entries[State->CurrentSelection]->Title)) >> 1,
            textPosY
        );
    }
    else {
        DrawTextWithTransparency (L"", 0, textPosY);
    }

    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
        DrawTextWithTransparency (
            Screen->Hint1,
            (ScreenW - egComputeTextWidth (Screen->Hint1)) / 2,
            ScreenH - (egGetFontHeight() * 3)
        );

        DrawTextWithTransparency (
            Screen->Hint2,
            (ScreenW - egComputeTextWidth (Screen->Hint2)) / 2,
            ScreenH - (egGetFontHeight() * 2)
        );
    }
} // static VOID PaintAll()

// Move the selection to State->CurrentSelection, adjusting icon row if necessary...
static
VOID PaintSelection (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    UINTN                *itemPosX,
    UINTN                 row0PosY,
    UINTN                 row1PosY,
    UINTN                 textPosY
) {
    UINTN XSelectPrev, XSelectCur, YPosPrev, YPosCur;

    if (((State->CurrentSelection <= State->LastVisible) &&
        (State->CurrentSelection >= State->FirstVisible)) ||
        (State->CurrentSelection >= State->InitialRow1)
    ) {
        if (Screen->Entries[State->PreviousSelection]->Row == 0) {
            XSelectPrev = State->PreviousSelection - State->FirstVisible;
            YPosPrev = row0PosY;
        }
        else {
            XSelectPrev = State->PreviousSelection;
            YPosPrev = row1PosY;
        }

        if (Screen->Entries[State->CurrentSelection]->Row == 0) {
            XSelectCur = State->CurrentSelection - State->FirstVisible;
            YPosCur = row0PosY;
        }
        else {
            XSelectCur = State->CurrentSelection;
            YPosCur = row1PosY;
        }

        DrawMainMenuEntry (
            Screen->Entries[State->PreviousSelection],
            FALSE,
            itemPosX[XSelectPrev],
            YPosPrev
        );

        DrawMainMenuEntry (
            Screen->Entries[State->CurrentSelection],
            TRUE,
            itemPosX[XSelectCur],
            YPosCur
        );

        if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL) &&
            (!PointerActive || (PointerActive && DrawSelection))
        ) {
            DrawTextWithTransparency (L"", 0, textPosY);
            DrawTextWithTransparency (
                Screen->Entries[State->CurrentSelection]->Title,
                (ScreenW - egComputeTextWidth (Screen->Entries[State->CurrentSelection]->Title)) >> 1,
                textPosY
            );
        }
        else {
            DrawTextWithTransparency (L"", 0, textPosY);
        }
    }
    else {
        // Current selection not visible; must redraw the menu...
        MainMenuStyle (Screen, State, MENU_FUNCTION_PAINT_ALL, NULL);
    }
} // static VOID MoveSelection (VOID)

// Display a 48x48 icon at the specified location. Uses the image specified by
// ExternalFilename if it is available, or BuiltInImage if it is not. The
// Y position is specified as the center value, and so is adjusted by half
// the icon's height. The X position is set along the icon's left
// edge if Alignment == ALIGN_LEFT, and along the right edge if
// Alignment == ALIGN_RIGHT
static
VOID PaintIcon (
    IN EG_EMBEDDED_IMAGE *BuiltInIcon,
    IN CHAR16            *ExternalFilename,
    UINTN                 PosX,
    UINTN                 PosY,
    UINTN                 Alignment
) {
    EG_IMAGE *Icon = NULL;

    Icon = egFindIcon (ExternalFilename, GlobalConfig.IconSizes[ICON_SIZE_SMALL]);
    if (Icon == NULL) {
        Icon = egPrepareEmbeddedImage (BuiltInIcon, TRUE);
    }

    if (Icon != NULL) {
        if (Alignment == ALIGN_RIGHT) {
            PosX -= Icon->Width;
        }

        egDrawImageWithTransparency (
            Icon,
            NULL,
            PosX,
            PosY - (Icon->Height / 2),
            Icon->Width,
            Icon->Height
        );

        egFreeImage (Icon);
    }
} // static VOID()

UINTN ComputeRow0PosY (VOID) {
    return ((ScreenH / 2) - TileSizes[0] / 2);
} // UINTN ComputeRow0PosY()

// Display (or erase) the arrow icons to the left and right of an icon's row,
// as appropriate.
static
VOID PaintArrows (
    SCROLL_STATE *State,
    UINTN         PosX,
    UINTN         PosY,
    UINTN         row0Loaders
) {
    EG_IMAGE *TempImage;
    UINTN Width, Height, RightX, AdjPosY;

    // NOTE: Assume that left and right arrows are of the same size...
    Width   = egemb_arrow_left.Width;
    Height  = egemb_arrow_left.Height;
    RightX  = (ScreenW + (TileSizes[0] + TILE_XSPACING) * State->MaxVisible) / 2 + TILE_XSPACING;
    AdjPosY = PosY - (Height / 2);

    // For PaintIcon() calls, the starting Y position is moved to the midpoint
    // of the surrounding row; PaintIcon() adjusts this back up by half the
    // icon's height to properly center it.
    if ((State->FirstVisible > 0) &&
        (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_ARROWS))
    ) {
        PaintIcon (&egemb_arrow_left, L"arrow_left", PosX, PosY, ALIGN_RIGHT);
    }
    else {
        TempImage = egCropImage (GlobalConfig.ScreenBackground, PosX - Width, AdjPosY, Width, Height);
        BltImage (TempImage, PosX - Width, AdjPosY);
        egFreeImage (TempImage);
    }

    if ((State->LastVisible < (row0Loaders - 1)) &&
        (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_ARROWS))
    ) {
        PaintIcon (&egemb_arrow_right, L"arrow_right", RightX, PosY, ALIGN_LEFT);
    }
    else {
        TempImage = egCropImage (GlobalConfig.ScreenBackground, RightX, AdjPosY, Width, Height);
        BltImage (TempImage, RightX, AdjPosY);
        egFreeImage (TempImage);
    }
} // VOID PaintArrows()

// Display main menu in graphics mode
VOID MainMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              Function,
    IN CHAR16            *ParamText
) {
           INTN   i;
           UINTN  row0Count, row1Count, row1PosX, row1PosXRunning;
    static UINTN  row0PosX, row0PosXRunning, row1PosY, row0Loaders;
    static UINTN *itemPosX;
    static UINTN  row0PosY, textPosY;

    State->ScrollMode = SCROLL_MODE_ICONS;
    switch (Function) {
        case MENU_FUNCTION_INIT:
            InitScroll (State, Screen->EntryCount, GlobalConfig.MaxTags);

            // layout
            row0Count = 0;
            row1Count = 0;
            row0Loaders = 0;
            for (i = 0; i <= State->MaxIndex; i++) {
                if (Screen->Entries[i]->Row == 1) {
                    row1Count++;
                }
                else {
                    row0Loaders++;
                    if (row0Count < State->MaxVisible) {
                        row0Count++;
                    }
                }
            }
            row0PosX = (ScreenW + TILE_XSPACING - (TileSizes[0] + TILE_XSPACING) * row0Count) >> 1;
            row0PosY = ComputeRow0PosY();
            row1PosX = (ScreenW + TILE_XSPACING - (TileSizes[1] + TILE_XSPACING) * row1Count) >> 1;
            row1PosY = row0PosY + TileSizes[0] + TILE_YSPACING;
            if (row1Count > 0) {
                textPosY = row1PosY + TileSizes[1] + TILE_YSPACING;
            }
            else {
                textPosY = row1PosY;
            }

            itemPosX = AllocatePool (sizeof (UINTN) * Screen->EntryCount);
            row0PosXRunning = row0PosX;
            row1PosXRunning = row1PosX;
            for (i = 0; i <= State->MaxIndex; i++) {
                if (Screen->Entries[i]->Row == 0) {
                    itemPosX[i] = row0PosXRunning;
                    row0PosXRunning += TileSizes[0] + TILE_XSPACING;
                }
                else {
                    itemPosX[i] = row1PosXRunning;
                    row1PosXRunning += TileSizes[1] + TILE_XSPACING;
                }
            }
            // initial painting
            InitSelection();
            SwitchToGraphicsAndClear (TRUE);
            break;

        case MENU_FUNCTION_CLEANUP:
            MyFreePool (&itemPosX);
            break;

        case MENU_FUNCTION_PAINT_ALL:
            PaintAll (Screen, State, itemPosX, row0PosY, row1PosY, textPosY);
            // For PaintArrows(), the starting Y position is moved to the midpoint
            // of the surrounding row; PaintIcon() adjusts this back up by half the
            // icon's height to properly center it.
            PaintArrows (State, row0PosX - TILE_XSPACING, row0PosY + (TileSizes[0] / 2), row0Loaders);
            break;

        case MENU_FUNCTION_PAINT_SELECTION:
            PaintSelection (Screen, State, itemPosX, row0PosY, row1PosY, textPosY);
            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) {
               DrawTextWithTransparency (L"", 0, textPosY + TextLineHeight());
               DrawTextWithTransparency (
                   ParamText,
                   (ScreenW - egComputeTextWidth (ParamText)) >> 1,
                   textPosY + TextLineHeight()
               );
            }
            break;
    }
} // VOID MainMenuStyle()

// Determines the index of the main menu item at the given coordinates.
UINTN FindMainMenuItem (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              PosX,
    IN UINTN              PosY
) {
           UINTN  i;
           UINTN  itemRow;
           UINTN  row0Count, row1Count, row1PosX, row1PosXRunning;
    static UINTN  row0PosX, row0PosXRunning, row1PosY, row0Loaders;
    static UINTN *itemPosX;
    static UINTN  row0PosY;

    row0Count = 0;
    row1Count = 0;
    row0Loaders = 0;
    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 1) {
            row1Count++;
        }
        else {
            row0Loaders++;
            if (row0Count < State->MaxVisible) {
                row0Count++;
            }
        }
    }
    row0PosX = (ScreenW + TILE_XSPACING - (TileSizes[0] + TILE_XSPACING) * row0Count) >> 1;
    row0PosY = ComputeRow0PosY();
    row1PosX = (ScreenW + TILE_XSPACING - (TileSizes[1] + TILE_XSPACING) * row1Count) >> 1;
    row1PosY = row0PosY + TileSizes[0] + TILE_YSPACING;

    if (PosY >= row0PosY && PosY <= row0PosY + TileSizes[0]) {
        itemRow = 0;
        if (PosX <= row0PosX) {
            return POINTER_LEFT_ARROW;
        }
        else if (PosX >= (ScreenW - row0PosX)) {
            return POINTER_RIGHT_ARROW;
        }
    }
    else if (PosY >= row1PosY && PosY <= row1PosY + TileSizes[1]) {
        itemRow = 1;
    }
    else {
        // Y coordinate is outside of either row
        return POINTER_NO_ITEM;
    }

    UINTN ItemIndex = POINTER_NO_ITEM;

    itemPosX = AllocatePool (sizeof (UINTN) * Screen->EntryCount);
    row0PosXRunning = row0PosX;
    row1PosXRunning = row1PosX;
    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0) {
            itemPosX[i] = row0PosXRunning;
            row0PosXRunning += TileSizes[0] + TILE_XSPACING;
        }
        else {
            itemPosX[i] = row1PosXRunning;
            row1PosXRunning += TileSizes[1] + TILE_XSPACING;
        }
    }

    for (i = State->FirstVisible; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0 && itemRow == 0) {
            if (i <= State->LastVisible) {
                if (PosX >= itemPosX[i - State->FirstVisible] &&
                    PosX <= itemPosX[i - State->FirstVisible] + TileSizes[0]
                ) {
                    ItemIndex = i;
                    break;
                }
            }
        }
        else if (Screen->Entries[i]->Row == 1 && itemRow == 1) {
            if (PosX >= itemPosX[i] && PosX <= itemPosX[i] + TileSizes[1]) {
                ItemIndex = i;
                break;
            }
        }
    }

    MyFreePool (&itemPosX);

    return ItemIndex;
} // VOID FindMainMenuItem()

////////////////////////////////////////////////////////////////////////////////
// Calculate center position of a menu entry for pointer positioning
////////////////////////////////////////////////////////////////////////////////
static
VOID GetMenuItemCenter (
    IN  REFIT_MENU_SCREEN *Screen,
    IN  SCROLL_STATE      *State,
    IN  UINTN              ItemIndex,
    OUT UINTN             *CenterX,
    OUT UINTN             *CenterY
) {
    UINTN  i;
    UINTN  row0PosX;
    UINTN  row0PosY;
    UINTN  row1PosX;
    UINTN  row1PosY;
    UINTN  itemPosX;
    UINTN  row0PosXRunning;
    UINTN  row1PosXRunning;
    if (ItemIndex > State->MaxIndex) {
        // Invalid index, default to screen center
        *CenterX = ScreenW >> 1;
        *CenterY = ScreenH >> 1;
        return;
    }
    // Need to get IconRowPosX, IconRowPosY, ToolRowPosX, ToolRowPosY, TileSizes from MainMenuStyle context
    // This is a simplified approach, assuming these are accessible or can be derived.
    // For a precise implementation, these static variables might need to be passed or made global.
    // For now, let's derive them from MainMenuStyle's logic (which is more complex than a simple call)
    // or assume they are exposed in some way.
    // Given the diff, there's a call to `GetStateInfo(Screen, State);` which is not provided.
    // I will assume `GetStateInfo` would populate the necessary static variables.
    // Since I don't have the `GetStateInfo` function, I will use placeholder logic based on the diff.

    // Placeholder for where these would come from (e.g., GetStateInfo)
    // For the purpose of this replacement, I'll use the logic from MainMenuStyle directly.
    UINTN row0Count, row1Count;
    row0Count = 0;
    row1Count = 0;
    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 1) {
            row1Count++;
        }
        else {
            if (row0Count < State->MaxVisible) {
                row0Count++;
            }
        }
    }
    row0PosX = (ScreenW + TILE_XSPACING - (TileSizes[0] + TILE_XSPACING) * row0Count) >> 1;
    row0PosY = ComputeRow0PosY(); // Assuming ComputeRow0PosY is accessible
    row1PosX = (ScreenW + TILE_XSPACING - (TileSizes[1] + TILE_XSPACING) * row1Count) >> 1;
    row1PosY = row0PosY + TileSizes[0] + TILE_YSPACING; // Assuming TileSizes are accessible

    // Re-calculate itemPosX as done in MainMenuStyle to get the correct x-coordinate for each item
    // This part is a bit tricky as itemPosX is usually static in MainMenuStyle.
    // We need to re-implement the layout calculation here.
    row0PosXRunning = row0PosX;
    row1PosXRunning = row1PosX;

    // This loop re-calculates the positions.
    for (i = 0; i <= State->MaxIndex; i++) {
        if (i == ItemIndex) { // Found the target item
            if (Screen->Entries[i]->Row == 0) {
                itemPosX = row0PosXRunning;
                *CenterX = itemPosX + (TileSizes[0] >> 1);
                *CenterY = row0PosY + (TileSizes[0] >> 1);
            }
            else {
                itemPosX = row1PosXRunning;
                *CenterX = itemPosX + (TileSizes[1] >> 1);
                *CenterY = row1PosY + (TileSizes[1] >> 1);
            }
            return;
        }
        if (Screen->Entries[i]->Row == 0) {
            row0PosXRunning += TileSizes[0] + TILE_XSPACING;
        }
        else {
            row1PosXRunning += TileSizes[1] + TILE_XSPACING;
        }
    }
    // Fallback to screen center if for some reason the item isn't found (should not happen with ItemIndex check)
    *CenterX = ScreenW >> 1;
    *CenterY = ScreenH >> 1;
} // static VOID GetMenuItemCenter()




// Enable the user to edit boot loader options.
// Returns TRUE if the user exited with edited options; FALSE if the user
// pressed Esc to terminate the edit.
static
BOOLEAN EditOptions (
    LOADER_ENTRY *MenuEntry
) {
    UINTN    x_max, y_max;
    CHAR16  *EditedOptions;
    BOOLEAN  retval = FALSE;

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
        return FALSE;
    }

    REFIT_CALL_4_WRAPPER(
        gST->ConOut->QueryMode, gST->ConOut,
        gST->ConOut->Mode->Mode,
        &x_max, &y_max
    );

    if (!GlobalConfig.TextOnly) {
        SwitchToText (TRUE);
    }

    if (line_edit (MenuEntry->LoadOptions, &EditedOptions, x_max)) {
        MyFreePool (&MenuEntry->LoadOptions);
        MenuEntry->LoadOptions = EditedOptions;

        retval = TRUE;
    }

    if (!GlobalConfig.TextOnly) {
        SwitchToGraphics();
    }

    return retval;
} // VOID EditOptions()

//
// user-callable dispatcher functions
//

VOID DisplaySimpleMessage (
    CHAR16 *Title,
    CHAR16 *Message
) {
    #if REFIT_DEBUG > 0
    LOG(4, LOG_THREE_STAR_MID, L"Entering DisplaySimpleMessage");
    #endif

    if (!Message) {
        return;
    }

    MENU_STYLE_FUNC      Style          = TextMenuStyle;
    INTN                 DefaultEntry   = 0;
    UINTN                MenuExit;
    REFIT_MENU_ENTRY    *ChosenOption;
    REFIT_MENU_ENTRY    *TempMenuEntry  = CopyMenuEntry (&MenuEntryReturn);
    TempMenuEntry->Image                = BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT);
    REFIT_MENU_SCREEN    HideItemMenu   = { NULL, NULL, 0, NULL, 0, &TempMenuEntry, 0, NULL,
                                         L"Press Enter to return to main menu", L"" };

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    HideItemMenu.TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT);
    HideItemMenu.Title      = Title;

    AddMenuInfoLine (&HideItemMenu, Message);
    AddMenuEntry (&HideItemMenu, &MenuEntryReturn);
    MenuExit = RunGenericMenu (&HideItemMenu, Style, &DefaultEntry, &ChosenOption);

    // DA-TAG: Tick box to run check after 'RunGenericMenu'
    CHAR16 *TypeMenuExit = NULL;
    if (MenuExit < 1) {
        // Dummy ... Never reached
        TypeMenuExit = L"UNKNOWN!!";
    }
    else {
        TypeMenuExit = MenuExitInfo (MenuExit);
    }

    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu call on '%s' in 'DisplaySimpleMessage'",
        MenuExit, TypeMenuExit, ChosenOption->Title
    );
    #endif
} // VOID DisplaySimpleMessage()

// Check each filename in FilenameList to be sure it refers to a valid file. If
// not, delete it. This works only on filenames that are complete, with volume,
// path, and filename components; if the filename omits the volume, the search
// is not done and the item is left intact, no matter what.
// Returns TRUE if any files were deleted, FALSE otherwise.
static
BOOLEAN RemoveInvalidFilenames (
    CHAR16 *FilenameList,
    CHAR16 *VarName
) {
    EFI_STATUS       Status;
    UINTN            i = 0;
    CHAR16          *Filename, *OneElement, *VolName = NULL;
    BOOLEAN          DeleteIt, DeletedSomething = FALSE;
    REFIT_VOLUME    *Volume;
    EFI_FILE_HANDLE  FileHandle;

    while ((OneElement = FindCommaDelimited (FilenameList, i)) != NULL) {
        DeleteIt = FALSE;
        Filename = StrDuplicate (OneElement);

        if (SplitVolumeAndFilename (&Filename, &VolName)) {
            DeleteIt = TRUE;

            if (FindVolume (&Volume, VolName) && Volume->RootDir) {
                Status = REFIT_CALL_5_WRAPPER(
                    Volume->RootDir->Open, Volume->RootDir,
                    &FileHandle, Filename,
                    EFI_FILE_MODE_READ, 0
                );

                if (Status == EFI_SUCCESS) {
                    DeleteIt = FALSE;
                    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
                }
            }
        }

        if (DeleteIt) {
            DeleteItemFromCsvList (OneElement, FilenameList);
        }
        else {
            i++;
        }

        MyFreePool (&OneElement);
        MyFreePool (&Filename);
        MyFreePool (&VolName);

        VolName = NULL;

        DeletedSomething |= DeleteIt;
    } // while

    return DeletedSomething;
} // BOOLEAN RemoveInvalidFilenames()

// Save a list of items to be hidden to NVRAM or disk,
// as determined by GlobalConfig.UseNvram.
static
VOID SaveHiddenList (
    IN CHAR16 *HiddenList,
    IN CHAR16 *VarName
) {
    EFI_STATUS Status;
    UINTN      ListLen;

    if (!HiddenList || !VarName) {
        // Prevent NULL dererencing
        Status = EFI_INVALID_PARAMETER;
    }
    else {
        ListLen = StrLen (HiddenList);

        Status = EfivarSetRaw (
            &RefindPlusGuid,
            VarName,
            HiddenList,
            ListLen * 2 + 2 * (ListLen > 0),
            TRUE
        );
    }

    CheckError (Status, L"in SaveHiddenList!!");
} // VOID SaveHiddenList()

// Present a menu that enables the user to delete hidden tags
//   that is, to un-hide them.
VOID ManageHiddenTags (VOID) {
    EFI_STATUS           Status  = EFI_SUCCESS;
    CHAR16              *AllTags = NULL, *OneElement = NULL;
    CHAR16              *HiddenLegacy, *HiddenFirmware, *HiddenTags, *HiddenTools;
    INTN                 DefaultEntry  = 0;
    UINTN                i             = 0;
    UINTN                MenuExit      = 0;
    BOOLEAN              SaveTags      = FALSE;
    BOOLEAN              SaveTools     = FALSE;
    BOOLEAN              SaveLegacy    = FALSE;
    BOOLEAN              SaveFirmware  = FALSE;
    MENU_STYLE_FUNC      Style         = TextMenuStyle;
    REFIT_MENU_ENTRY    *ChosenOption  = NULL;
    REFIT_MENU_ENTRY    *MenuEntryItem = NULL;

    CHAR16             *MenuInfo     = L"Select a tag and press Enter to restore it";
    REFIT_MENU_SCREEN   HideItemMenu = { L"Manage Hidden Tags", NULL, 0, &MenuInfo, 0, NULL, 0, NULL,
                                         L"Select an option and press Enter or",
                                         L"press Esc to return to main menu without changes" };

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_SEPARATOR, L"Managing hidden tags");
    #endif

    HideItemMenu.TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN);
    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    HiddenTags = ReadHiddenTags (L"HiddenTags");
    if (HiddenTags) {
        SaveTags = RemoveInvalidFilenames (HiddenTags, L"HiddenTags");
        if (HiddenTags && (HiddenTags[0] != L'\0')) {
            AllTags = StrDuplicate (HiddenTags);
        }
    }

    HiddenTools = ReadHiddenTags (L"HiddenTools");
    if (HiddenTools) {
        SaveTools = RemoveInvalidFilenames (HiddenTools, L"HiddenTools");
        if (HiddenTools && (HiddenTools[0] != L'\0')) {
            MergeStrings (&AllTags, HiddenTools, L',');
        }
    }

    HiddenLegacy = ReadHiddenTags (L"HiddenLegacy");
    if (HiddenLegacy && (HiddenLegacy[0] != L'\0')) {
        MergeStrings (&AllTags, HiddenLegacy, L',');
    }

    HiddenFirmware = ReadHiddenTags (L"HiddenFirmware");
    if (HiddenFirmware && (HiddenFirmware[0] != L'\0')) {
        MergeStrings (&AllTags, HiddenFirmware, L',');
    }

    if (!AllTags || StrLen (AllTags) < 1) {
        DisplaySimpleMessage (L"Information", L"No hidden tags found");
    }
    else {
        AddMenuInfoLine (&HideItemMenu, StrDuplicate (MenuInfo));
        while ((OneElement = FindCommaDelimited (AllTags, i++)) != NULL) {
            MenuEntryItem = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY)); // do not free
            MenuEntryItem->Title = StrDuplicate (OneElement);
            MenuEntryItem->Tag   = TAG_RETURN;
            MenuEntryItem->Row   = 1;
            AddMenuEntry (&HideItemMenu, MenuEntryItem);
            MyFreePool (&OneElement);
        } // while

        MenuExit = RunGenericMenu (&HideItemMenu, Style, &DefaultEntry, &ChosenOption);

        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL,
            L"Returned '%d' (%s) from RunGenericMenu call on '%s' in 'ManageHiddenTags'",
            MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
        );
        #endif

        if (MenuExit == MENU_EXIT_ENTER) {
            SaveTags     |= DeleteItemFromCsvList (ChosenOption->Title, HiddenTags);
            SaveTools    |= DeleteItemFromCsvList (ChosenOption->Title, HiddenTools);
            SaveFirmware |= DeleteItemFromCsvList (ChosenOption->Title, HiddenFirmware);
            SaveLegacy   |= DeleteItemFromCsvList (ChosenOption->Title, HiddenLegacy);

            if (DeleteItemFromCsvList (ChosenOption->Title, HiddenLegacy)) {
                i = HiddenLegacy ? StrLen (HiddenLegacy) : 0;
                Status = EfivarSetRaw (
                    &RefindPlusGuid,
                    L"HiddenLegacy",
                    HiddenLegacy,
                    i * 2 + 2 * (i > 0),
                    TRUE
                );
                SaveLegacy = TRUE;
                CheckError (Status, L"in ManageHiddenTags!!");
            }
        }

        if (SaveTags) {
            SaveHiddenList (HiddenTags, L"HiddenTags");
        }

        if (SaveLegacy) {
            SaveHiddenList (HiddenLegacy, L"HiddenLegacy");
        }

        if (SaveTools) {
            SaveHiddenList (HiddenTools, L"HiddenTools");
            MyFreePool (&gHiddenTools);
            gHiddenTools = NULL;
        }

        if (SaveFirmware) {
            SaveHiddenList (HiddenFirmware, L"HiddenFirmware");
        }

        if (SaveTags || SaveTools || SaveLegacy || SaveFirmware) {
            RescanAll (FALSE, FALSE);
        }
    } // if !AllTags

    MyFreePool (&AllTags);
    MyFreePool (&HiddenTags);
    MyFreePool (&HiddenTools);
    MyFreePool (&HiddenLegacy);
    MyFreePool (&HiddenFirmware);
} // VOID ManageHiddenTags()

CHAR16 * ReadHiddenTags (
    CHAR16 *VarName
) {
    CHAR16     *Buffer = NULL;
    UINTN       Size;
    EFI_STATUS  Status;

    Status = EfivarGetRaw (&RefindPlusGuid, VarName, (VOID **) &Buffer, &Size);

    #if REFIT_DEBUG > 0
    if ((Status != EFI_SUCCESS) && (Status != EFI_NOT_FOUND)) {
        CHAR16 *CheckErrMsg = PoolPrint (L"in ReadHiddenTags ('%s')", VarName);
        CheckError (Status, CheckErrMsg);
        MyFreePool (&CheckErrMsg);
    }
    #endif

    if ((Status == EFI_SUCCESS) && (Size == 0)) {
        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL,
            L"Zero Size in ReadHiddenTags ... Clearing Buffer"
        );
        #endif

        MyFreePool (&Buffer);
        Buffer = NULL;
    }

    return Buffer;
} // CHAR16* ReadHiddenTags()

// Add PathName to the hidden tags variable specified by *VarName.
static
VOID AddToHiddenTags (CHAR16 *VarName, CHAR16 *Pathname) {
    EFI_STATUS  Status;
    CHAR16     *HiddenTags;

    if (Pathname && (StrLen (Pathname) > 0)) {
        HiddenTags = ReadHiddenTags (VarName);
        if (!HiddenTags) {
            // Prevent NULL dererencing
            HiddenTags = StrDuplicate (Pathname);
        }
        else {
            MergeStrings (&HiddenTags, Pathname, L',');
        }

        Status = EfivarSetRaw (
            &RefindPlusGuid,
            VarName,
            HiddenTags,
            StrLen (HiddenTags) * 2 + 2,
            TRUE
        );

        CheckError (Status, L"in AddToHiddenTags!!");
        MyFreePool (&HiddenTags);
    }
} // VOID AddToHiddenTags()

// Adds a filename, specified by the *Loader variable, to the *VarName EFI variable,
// using the mostly-prepared *HideItemMenu structure to prompt the user to confirm
// hiding that item.
// Returns TRUE if item was hidden, FALSE otherwise.
static
BOOLEAN HideEfiTag (
    LOADER_ENTRY      *Loader,
    REFIT_MENU_SCREEN *HideItemMenu,
    CHAR16            *VarName
) {
    REFIT_VOLUME      *TestVolume   = NULL;
    BOOLEAN            TagHidden    = FALSE;
    CHAR16            *FullPath     = NULL;
    CHAR16            *GuidStr      = NULL;
    UINTN              MenuExit;
    INTN               DefaultEntry = 1;
    MENU_STYLE_FUNC    Style        = TextMenuStyle;
    REFIT_MENU_ENTRY  *ChosenOption;

    if (!Loader ||
        !VarName ||
        !HideItemMenu ||
        !Loader->Volume ||
        !Loader->LoaderPath
    ) {
        return FALSE;
    }

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    if (Loader->Volume->VolName && (StrLen (Loader->Volume->VolName) > 0)) {
        FullPath = StrDuplicate (Loader->Volume->VolName);
    }

    MergeStrings (&FullPath, Loader->LoaderPath, L':');
    AddMenuInfoLine (HideItemMenu, PoolPrint (L"Are you sure you want to hide %s?", FullPath));
    AddMenuEntry (HideItemMenu, &MenuEntryYes);
    AddMenuEntry (HideItemMenu, &MenuEntryNo);

    MenuExit = RunGenericMenu (HideItemMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu call on '%s' in 'HideEfiTag'",
        MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
    );
    #endif

    if (MyStriCmp (ChosenOption->Title, L"Yes") && (MenuExit == MENU_EXIT_ENTER)) {
        GuidStr = GuidAsString (&Loader->Volume->PartGuid);
        if (FindVolume (&TestVolume, GuidStr) && TestVolume->RootDir) {
            MyFreePool (&FullPath);
            FullPath = NULL;
            MergeStrings (&FullPath, GuidStr, L'\0');
            MergeStrings (&FullPath, L":", L'\0');
            MergeStrings (
                &FullPath,
                Loader->LoaderPath,
                (Loader->LoaderPath[0] == L'\\' ? L'\0' : L'\\')
            );
        }
        AddToHiddenTags (VarName, FullPath);
        TagHidden = TRUE;
        MyFreePool (&GuidStr);
    }

    MyFreePool (&FullPath);

    return TagHidden;
} // BOOLEAN HideEfiTag()

static
BOOLEAN HideFirmwareTag(
    LOADER_ENTRY      *Loader,
    REFIT_MENU_SCREEN *HideItemMenu
) {
    MENU_STYLE_FUNC     Style = TextMenuStyle;
    REFIT_MENU_ENTRY   *ChosenOption;
    INTN                DefaultEntry = 1;
    UINTN               MenuExit;
    BOOLEAN             TagHidden = FALSE;

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    AddMenuInfoLine(HideItemMenu, PoolPrint(L"Really hide '%s'?", Loader->Title));
    AddMenuEntry(HideItemMenu, &MenuEntryYes);
    AddMenuEntry(HideItemMenu, &MenuEntryNo);

    MenuExit = RunGenericMenu(
        HideItemMenu,
        Style,
        &DefaultEntry,
        &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu call on '%s' in 'HideFirmwareTag'",
        MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
    );
    #endif

    if ((MyStriCmp(ChosenOption->Title, L"Yes")) &&
        (MenuExit == MENU_EXIT_ENTER)
    ) {
        AddToHiddenTags(L"HiddenFirmware", Loader->Title);
        TagHidden = TRUE;
    }

    return TagHidden;
} // BOOLEAN HideFirmwareTag()


static
BOOLEAN HideLegacyTag (
    LEGACY_ENTRY      *LegacyLoader,
    REFIT_MENU_SCREEN *HideItemMenu
) {
    MENU_STYLE_FUNC    Style = TextMenuStyle;
    REFIT_MENU_ENTRY   *ChosenOption;
    INTN                DefaultEntry = 1;
    UINTN               MenuExit;
    CHAR16             *Name      = NULL;
    BOOLEAN             TagHidden = FALSE;

    if (AllowGraphicsMode)
        Style = GraphicsMenuStyle;

    if ((GlobalConfig.LegacyType == LEGACY_TYPE_MAC) && LegacyLoader->me.Title) {
        Name = StrDuplicate (LegacyLoader->me.Title);
    }
    if ((GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) &&
        LegacyLoader->BdsOption && LegacyLoader->BdsOption->Description
    ) {
        Name = StrDuplicate (LegacyLoader->BdsOption->Description);
    }
    if (!Name) {
        Name = StrDuplicate (L"Legacy (BIOS) OS");
    }
    AddMenuInfoLine (HideItemMenu, PoolPrint (L"Are you sure you want to hide '%s'?", Name));
    AddMenuEntry (HideItemMenu, &MenuEntryYes);
    AddMenuEntry (HideItemMenu, &MenuEntryNo);
    MenuExit = RunGenericMenu (HideItemMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu call on '%s' in 'HideLegacyTag'",
        MenuExit, MenuExitInfo (MenuExit), ChosenOption->Title
    );
    #endif

    if (MyStriCmp (ChosenOption->Title, L"Yes") && (MenuExit == MENU_EXIT_ENTER)) {
        AddToHiddenTags (L"HiddenLegacy", Name);
        TagHidden = TRUE;
    }
    MyFreePool (&Name);

    return TagHidden;
} // BOOLEAN HideLegacyTag()

static
VOID HideTag (
    REFIT_MENU_ENTRY *ChosenEntry
) {
    LOADER_ENTRY      *Loader        = (LOADER_ENTRY *) ChosenEntry;
    LEGACY_ENTRY      *LegacyLoader  = (LEGACY_ENTRY *) ChosenEntry;
    REFIT_MENU_SCREEN  HideItemMenu  = {
        NULL, NULL, 0, NULL, 0, NULL, 0, NULL,
        L"Select an option and press 'Enter' or",
        L"press 'Esc' to return to main menu without changes"
    };

    if (ChosenEntry == NULL) {
        return;
    }

    HideItemMenu.TitleImage = BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN);
    // BUG: The RescanAll() calls should be conditional on successful calls to
    // HideEfiTag() or HideLegacyTag(); but for the former, this causes
    // crashes on a second call hide a tag if the user chose "No" to the first
    // call. This seems to be related to memory management of Volumes; the
    // crash occurs in FindVolumeAndFilename() and lib.c when calling
    // DevicePathToStr(). Calling RescanAll() on all returns from HideEfiTag()
    // seems to be an effective workaround, but there's likely a memory
    // management bug somewhere that is the root cause.
    switch (ChosenEntry->Tag) {
        case TAG_LOADER:
            if (Loader->DiscoveryType == DISCOVERY_TYPE_AUTO) {
                HideItemMenu.Title = L"Hide EFI OS Tag";
                HideEfiTag (Loader, &HideItemMenu, L"HiddenTags");

                #if REFIT_DEBUG > 0
                MsgLog ("User Input Received:\n");
                MsgLog ("  - %s\n\n", HideItemMenu.Title);
                #endif

                RescanAll (FALSE, FALSE);
            }
            else {
                DisplaySimpleMessage (
                    L"Cannot Hide Entry for Manual Boot Stanza",
                    L"You must edit 'config.conf' to remove this entry."
                );
            }
            break;

        case TAG_LEGACY:
        case TAG_LEGACY_UEFI:
            HideItemMenu.Title = L"Hide Legacy (BIOS) OS Tag";
            if (HideLegacyTag (LegacyLoader, &HideItemMenu)) {
                #if REFIT_DEBUG > 0
                MsgLog ("User Input Received:\n");
                MsgLog ("  - %s\n\n", HideItemMenu.Title);
                #endif

                RescanAll (FALSE, FALSE);
            }
            break;

        case TAG_FIRMWARE_LOADER:
            HideItemMenu.Title = L"Hide Firmware Boot Option Tag";
            if (HideFirmwareTag(Loader, &HideItemMenu)) {
                RescanAll(FALSE, FALSE);
            }
            break;

        case TAG_ABOUT:
        case TAG_REBOOT:
        case TAG_SHUTDOWN:
        case TAG_EXIT:
        case TAG_FIRMWARE:
        case TAG_CSR_ROTATE:
        case TAG_INSTALL:
        case TAG_HIDDEN:
            DisplaySimpleMessage (
                L"Unable to Comply",
                L"To hide an internal tool, edit the 'showtools' line in config.conf"
            );
            break;

        case TAG_TOOL:
            HideItemMenu.Title = L"Hide Tool Tag";
            HideEfiTag (Loader, &HideItemMenu, L"HiddenTools");
            MyFreePool (&gHiddenTools);
            gHiddenTools = NULL;

            #if REFIT_DEBUG > 0
            MsgLog ("User Input Received:\n");
            MsgLog ("  - %s\n\n", HideItemMenu.Title);
            #endif

            RescanAll (FALSE, FALSE);
            break;
    } // switch
} // VOID HideTag()

UINTN RunMenu (
    IN  REFIT_MENU_SCREEN  *Screen,
    OUT REFIT_MENU_ENTRY  **ChosenEntry
) {
    INTN            DefaultEntry = -1;
    UINTN           MenuExit;
    MENU_STYLE_FUNC Style        = TextMenuStyle;

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    MenuExit = RunGenericMenu (Screen, Style, &DefaultEntry, ChosenEntry);

    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu call on '%s' in 'RunMenu'",
        MenuExit, MenuExitInfo (MenuExit), Screen->Title
    );
    #endif

    return MenuExit;
} // UINTN RunMenu()

UINTN RunMainMenu (
    REFIT_MENU_SCREEN  *Screen,
    CHAR16            **DefaultSelection,
    REFIT_MENU_ENTRY  **ChosenEntry
) {
    REFIT_MENU_ENTRY   *TempChosenEntry     = NULL;
    MENU_STYLE_FUNC     Style               = TextMenuStyle;
    MENU_STYLE_FUNC     MainStyle           = TextMenuStyle;
    CHAR16             *MenuTitle;
    UINTN               MenuExit            = 0;
    INTN                DefaultEntryIndex   = -1;
    INTN                DefaultSubmenuIndex = -1;

    // remove any buffered key strokes
    ReadAllKeyStrokes();

    TileSizes[0] = (GlobalConfig.IconSizes[ICON_SIZE_BIG] * 9) / 8;
    TileSizes[1] = (GlobalConfig.IconSizes[ICON_SIZE_SMALL] * 4) / 3;

    if ((DefaultSelection != NULL) && (*DefaultSelection != NULL)) {
        // Find a menu entry that includes *DefaultSelection as a substring
        DefaultEntryIndex = FindMenuShortcutEntry (Screen, *DefaultSelection);
    }

    if (AllowGraphicsMode) {
        Style     = GraphicsMenuStyle;
        MainStyle = MainMenuStyle;

        PointerEnabled = PointerActive = pdAvailable();
//        if (Screen->TimeoutSeconds > 0) {DrawSelection = !PointerEnabled;}
//        else {DrawSelection = TRUE;}
        DrawSelection = TRUE;
    }

    // Generate this now and keep it around forever, since it is likely to be
    // used after this function terminates.
    
    MenuTitle = StrDuplicate (L"Unknown");

    while (!MenuExit) {
        MenuExit = RunGenericMenu (Screen, MainStyle, &DefaultEntryIndex, &TempChosenEntry);

        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL,
            L"Returned '%d' (%s) from RunGenericMenu call on '%s' in 'RunMainMenu'",
            MenuExit, MenuExitInfo (MenuExit), TempChosenEntry->Title
        );
        #endif

        Screen->TimeoutSeconds = 0;

        MyFreePool (&MenuTitle);
        MenuTitle = StrDuplicate (TempChosenEntry->Title);
        if (MenuExit == MENU_EXIT_DETAILS) {
            if (!TempChosenEntry->SubScreen) {
                // no sub-screen; ignore keypress
                MenuExit = 0;
            }
            else {
                MenuExit = RunGenericMenu (
                    TempChosenEntry->SubScreen,
                    Style,
                    &DefaultSubmenuIndex,
                    &TempChosenEntry
                );

                #if REFIT_DEBUG > 0
                LOG(2, LOG_LINE_NORMAL,
                    L"Returned '%d' (%s) from RunGenericMenu call on SubScreen in 'RunMainMenu'",
                    MenuExit, MenuExitInfo (MenuExit)
                );
                #endif

               if (MenuExit == MENU_EXIT_ESCAPE || TempChosenEntry->Tag == TAG_RETURN) {
                   MenuExit = 0;
               }

               if (MenuExit == MENU_EXIT_DETAILS) {
                  if (!EditOptions ((LOADER_ENTRY *) TempChosenEntry)) {
                      MenuExit = 0;
                  }
               }
            }
        } // if MenuExit == MENU_EXIT_DETAILS

        if (MenuExit == MENU_EXIT_HIDE) {
            if (GlobalConfig.HiddenTags) {
                HideTag (TempChosenEntry);
            }
            MenuExit = 0;
        }
       REFIT_CALL_1_WRAPPER(gBS->Stall, 15000); 
    } // while

    if (ChosenEntry) {
        *ChosenEntry = TempChosenEntry;
    }

    if (DefaultSelection) {
       ReleasePtr (*DefaultSelection);
       *DefaultSelection = MenuTitle;
    }

    return MenuExit;
} // UINTN RunMainMenu()

VOID FreeLoaderEntry (
    IN LOADER_ENTRY *Entry
) {
    egFreeImage (Entry->me.Image);
    MyFreePool (&Entry->EfiLoaderPath);
    MyFreePool (&Entry->LoadOptions);
    MyFreePool (&Entry->InitrdPath);
    MyFreePool (&Entry->LoaderPath);
    MyFreePool (&Entry->me.Title);
    MyFreePool (&Entry->Title);

    MyFreePool (&Entry);
} // VOID FreeLoaderEntry()

BDS_COMMON_OPTION * CopyBdsOption (
    BDS_COMMON_OPTION *BdsOption
) {
    BDS_COMMON_OPTION *NewBdsOption = NULL;

    if (BdsOption) {
        NewBdsOption = AllocateCopyPool (sizeof (*BdsOption), BdsOption);
        if (NewBdsOption) {
            if (BdsOption->DevicePath) {
                NewBdsOption->DevicePath = AllocateCopyPool (
                    GetDevicePathSize (BdsOption->DevicePath),
                    BdsOption->DevicePath
                );
            }

            if (BdsOption->OptionName) {
                NewBdsOption->OptionName = AllocateCopyPool (
                    StrSize (BdsOption->OptionName),
                    BdsOption->OptionName
                );
            }

            if (BdsOption->Description) {
                NewBdsOption->Description = AllocateCopyPool (
                    StrSize (BdsOption->Description),
                    BdsOption->Description
                );
            }

            if (BdsOption->LoadOptions) {
                NewBdsOption->LoadOptions = AllocateCopyPool (
                    BdsOption->LoadOptionsSize,
                    BdsOption->LoadOptions
                );
            }

            if (BdsOption->StatusString) {
                NewBdsOption->StatusString = AllocateCopyPool (
                    StrSize (BdsOption->StatusString),
                    BdsOption->StatusString
                );
            }
        }
    } // if NewBdsOption()

    return NewBdsOption;
} // BDS_COMMON_OPTION * CopyBdsOption()

VOID FreeBdsOption (
    BDS_COMMON_OPTION **BdsOption
) {
    if (BdsOption && *BdsOption) {
        ReleasePtr ((*BdsOption)->DevicePath);
        ReleasePtr ((*BdsOption)->OptionName);
        ReleasePtr ((*BdsOption)->Description);
        ReleasePtr ((*BdsOption)->LoadOptions);
        ReleasePtr ((*BdsOption)->StatusString);

        ReleasePtr (*BdsOption);
    }
} // VOID FreeBdsOption()
