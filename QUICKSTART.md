# UE5 Agent Python — Quickstart

Two-minute walkthrough from zero to first prompt.

## 1. Drop the plugin into your project

Copy the `UE5AgentPython` folder into `<YourProject>/Plugins/`. Right-click the `.uproject` → **Generate Visual Studio project files**.

## 2. Build

Open the `.sln` in Visual Studio 2022, select **Development Editor / Win64**, and build. Launch the editor. Accept any prompt to enable `PythonScriptPlugin` and `EditorScriptingUtilities`.

## 3. Open the panel

**Tools → UE5 Agent Python.** Dock the nomad tab wherever you like.

## 4. Configure a provider

Click the ⚙️ gear to expand settings. Pick a provider (Anthropic Claude / OpenAI / Google Gemini), paste your API key, press **Enter**. The status flips to `N models` and the model combo populates. Pick a model. Collapse the gear.

## 5. Try it

Select a few actors in the viewport, type:

> *Move the selected actors into a World Outliner folder named "FirstTest".*

Hit **Execute**. Watch the **Running…** throbber. When it clears, check the outliner. Click the 📋 log icon for the full trace.

## 6. Iterate

Flip **Conversation mode** on and keep going:

> *Now duplicate each of them 500 units to the right.*

The model remembers the prior turn.

---

Full feature reference: [README.md](README.md).
