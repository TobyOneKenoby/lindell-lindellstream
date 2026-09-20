#!/bin/bash
set -euo pipefail
umask 022
BASE="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
KEYCHAIN=""
cleanup() {
  if [ -n "$KEYCHAIN" ]; then security delete-keychain "$KEYCHAIN" >/dev/null 2>&1 || true; fi
  rm -rf "$WORK"
}
trap cleanup EXIT
cd "$BASE"
mkdir -p installer-output
python3 - <<'PY'
import os
names = ['MAC_CERTIFICATES_P12_BASE64','MAC_CERTIFICATES_PASSWORD','MAC_APPLICATION_IDENTITY','MAC_INSTALLER_IDENTITY','APPLE_ID','APPLE_TEAM_ID','APPLE_APP_PASSWORD']
missing = [n for n in names if not os.environ.get(n)]
mode = 'unsigned' if missing else 'signed'
with open('installer-output/SIGNING-STATUS.txt','w') as f:
    f.write('Signing credentials incomplete; UNSIGNED DRAFT, not ready for beta distribution.\nMissing GitHub secrets: ' + ', '.join(missing) + '\n' if missing else 'Signing credentials present; notarization and Gatekeeper checks must pass.\n')
with open(os.environ['GITHUB_OUTPUT'],'a') as f: f.write('mode=' + mode + '\n')
print('Missing signing secret names: ' + ', '.join(missing) if missing else 'All required signing credentials are configured.')
PY
MODE=unsigned
if ! grep -q 'UNSIGNED DRAFT' installer-output/SIGNING-STATUS.txt; then MODE=signed; fi
mkdir -p "$WORK/input" "$WORK/root/Library/Audio/Plug-Ins/VST3" "$WORK/root/Library/Application Support/Lindell Streams Live"
ditto -x -k incoming/Lindell-Streams-Mac-VST3.zip "$WORK/input"
PLUGIN="$WORK/root/Library/Audio/Plug-Ins/VST3/Lindell Streams Live.vst3"
ditto "$WORK/input/release/Lindell Streams Live.vst3" "$PLUGIN"
ditto "$WORK/input/release/ThirdParty" "$WORK/root/Library/Application Support/Lindell Streams Live/ThirdParty"
cp "$WORK/input/release/INSTALL-PLUGIN.txt" "$WORK/input/release/BUILD-INFO.txt" "$WORK/root/Library/Application Support/Lindell Streams Live/"
# No installer scripts or Gatekeeper/quarantine changes are performed.
lipo "$PLUGIN/Contents/MacOS/Lindell Streams Live" -verify_arch arm64 x86_64
if [ "$MODE" = signed ]; then
  KEYCHAIN="$WORK/signing.keychain-db"
  KEYCHAIN_PASSWORD="$(openssl rand -hex 24)"
  export P12_PATH="$WORK/certificates.p12"
  python3 - <<'PY'
import base64,os
with open(os.environ['P12_PATH'],'wb') as f: f.write(base64.b64decode(os.environ['MAC_CERTIFICATES_P12_BASE64'], validate=True))
os.chmod(os.environ['P12_PATH'],0o600)
PY
  security create-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
  security set-keychain-settings -lut 21600 "$KEYCHAIN"
  security unlock-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
  security import "$P12_PATH" -k "$KEYCHAIN" -P "$MAC_CERTIFICATES_PASSWORD" -T /usr/bin/codesign -T /usr/bin/productsign
  security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$KEYCHAIN_PASSWORD" "$KEYCHAIN" >/dev/null
  codesign --force --sign "$MAC_APPLICATION_IDENTITY" --keychain "$KEYCHAIN" --options runtime --timestamp "$PLUGIN"
fi
codesign --verify --strict --verbose=2 "$PLUGIN"
pkgbuild --analyze --root "$WORK/root" "$WORK/components.plist"
python3 - "$WORK/components.plist" <<'PY'
import plistlib,sys
p=sys.argv[1]
with open(p,'rb') as f: components=plistlib.load(f)
for c in components:
    c['BundleIsRelocatable']=False
    c['BundleOverwriteAction']='upgrade'
with open(p,'wb') as f: plistlib.dump(components,f)
PY
pkgbuild --root "$WORK/root" --component-plist "$WORK/components.plist" --identifier com.lindellstreams.live.vst3.pkg --version 0.3.1 --install-location / --ownership recommended "$WORK/LindellStreamsLive-component.pkg"
productbuild --distribution Installer/Distribution.xml --resources Installer/Resources --package-path "$WORK" "$WORK/product.pkg"
if [ "$MODE" = signed ]; then
  FINAL="$BASE/installer-output/Lindell-Streams-Live-0.3.1.pkg"
  productsign --sign "$MAC_INSTALLER_IDENTITY" --keychain "$KEYCHAIN" --timestamp "$WORK/product.pkg" "$FINAL"
  pkgutil --check-signature "$FINAL"
  xcrun notarytool submit "$FINAL" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD" --wait --timeout 20m --output-format json > "$WORK/notarization.json"
  python3 - "$WORK/notarization.json" <<'PY'
import json,sys
with open(sys.argv[1]) as f: r=json.load(f)
if r.get('status')!='Accepted': raise SystemExit('Apple did not accept notarization; no beta package will be published.')
print('Apple notarization accepted.')
PY
  xcrun stapler staple "$FINAL"
  xcrun stapler validate "$FINAL"
  spctl --assess --type install --verbose=2 "$FINAL"
  printf 'Developer ID signed; Apple notarization accepted; ticket stapled; Gatekeeper installation assessment passed.\n' > installer-output/SIGNING-STATUS.txt
else
  FINAL="$BASE/installer-output/Lindell-Streams-Live-0.3.1-UNSIGNED-DRAFT.pkg"
  cp "$WORK/product.pkg" "$FINAL"
fi
# Install on the disposable macOS runner and verify the actual installed payload.
sudo installer -pkg "$FINAL" -target /
cmp "$PLUGIN/Contents/MacOS/Lindell Streams Live" '/Library/Audio/Plug-Ins/VST3/Lindell Streams Live.vst3/Contents/MacOS/Lindell Streams Live'
codesign --verify --strict '/Library/Audio/Plug-Ins/VST3/Lindell Streams Live.vst3'
pkgutil --pkg-info com.lindellstreams.live.vst3.pkg
printf '\nInstaller payload and system installation verified on macOS CI.\n' >> installer-output/SIGNING-STATUS.txt
shasum -a 256 "$FINAL" > installer-output/SHA256.txt
