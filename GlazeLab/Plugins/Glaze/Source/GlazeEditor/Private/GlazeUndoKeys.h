#pragma once

#include "CoreMinimal.h"
#include "Editor.h"
#include "Input/Events.h"

namespace Glaze
{
	/** Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z in a Glaze window: the editor's undo does not get keys from standalone windows. */
	inline bool HandleUndoKeys(const FKeyEvent& KeyEvent)
	{
		if (!GEditor || !KeyEvent.IsControlDown() || KeyEvent.IsAltDown())
		{
			return false;
		}
		const FKey Key = KeyEvent.GetKey();
		if (Key == EKeys::Z && !KeyEvent.IsShiftDown())
		{
			GEditor->UndoTransaction();
			return true;
		}
		if (Key == EKeys::Y || (Key == EKeys::Z && KeyEvent.IsShiftDown()))
		{
			GEditor->RedoTransaction();
			return true;
		}
		return false;
	}
}
