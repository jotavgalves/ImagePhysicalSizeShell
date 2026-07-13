# Known Risks

- Windows property-handler registration can vary by Windows build and installed software.
- Replacing a property handler destructively can break native metadata. The project must use a proxy/decorator and abort if delegation cannot be proven.
- Property list values may be inherited from ProgIDs or `SystemFileAssociations`. Installers must preserve exact previous state.
- Explorer can keep COM DLLs loaded. Uninstall must tolerate locked files and schedule deletion when required.

