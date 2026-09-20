# macOS beta installer

Packages the tested universal 0.3.1 artifact from build run 35478954205. Installs
Lindell Streams Live.vst3 to /Library/Audio/Plug-Ins/VST3/ for all users and places
instructions and notices under /Library/Application Support/Lindell Streams Live/.
Administrator authorization is required. No postinstall scripts, quarantine removal,
or changes to macOS security settings. Existing system copies are upgraded in place.
A separate copy in a user's ~/Library/Audio/Plug-Ins/VST3/ folder is not removed.

The workflow tests an actual installation on a disposable macOS runner. With signing
credentials it signs the plugin using Developer ID Application and the package using
Developer ID Installer, submits to Apple, staples the accepted ticket and checks
Gatekeeper. No signed release artifact is published if any of those steps fails.
Without all credentials it creates an explicitly UNSIGNED DRAFT for installer review.
That draft does not solve Gatekeeper and should not be sent as the approved beta.

## Configure once in GitHub repository Settings > Secrets and variables > Actions

- MAC_CERTIFICATES_P12_BASE64: base64 of a password-protected .p12 export containing
  the Developer ID Application and Developer ID Installer certificates AND private keys.
- MAC_CERTIFICATES_PASSWORD: password protecting that .p12.
- MAC_APPLICATION_IDENTITY: full Developer ID Application identity including team ID.
- MAC_INSTALLER_IDENTITY: full Developer ID Installer identity including team ID.
- APPLE_ID: Apple account used for notarization.
- APPLE_TEAM_ID: that account's developer team identifier.
- APPLE_APP_PASSWORD: Apple app-specific password for notarization, not the account password.

Enter credentials directly in GitHub Actions secrets; never in source files or chat.
Use signing identities you are authorized to distribute this product under. Certificates
are imported into a temporary runner keychain and removed on exit. Then run Actions >
Package macOS beta installer > Run workflow. Download the `signed` artifact only after
all checks pass. Ordinary Installer/admin prompts can still appear on recipients' Macs.

The pinned build artifact expires after 30 days. Before a later release, select a newly
verified build and update both its run ID and artifact name here and in the workflow.

Apple documentation:
https://developer.apple.com/help/account/certificates/create-developer-id-certificates/
https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution
