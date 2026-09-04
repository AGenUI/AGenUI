#!/usr/bin/env node
/**
 * Prepares HarmonyOS release signing material for a headless hvigor build.
 *
 * hvigor refuses plaintext passwords in build-profile.json5: it requires the
 * AES-128-GCM ciphertext produced by DevEco Studio, and derives the key from a
 * `material/{fd,ac,ce}` directory sitting next to the .p12 (see
 * hvigor-ohos-plugin/src/utils/decipher-util.js). This script reproduces that
 * format so a release .app can be built without opening the IDE.
 *
 * Reads a JSON request on stdin (passwords never touch argv, where `ps` would
 * expose them) and writes a JSON result to stdout.
 *
 * Request:
 *   workDir       scratch dir; receives copies of the signing files + material/
 *   buildProfile  path to playground/harmony/build-profile.json5
 *   appJson5      path to playground/harmony/AppScope/app.json5
 *   storeFile     release .p12
 *   certPath      release .cer
 *   profilePath   release .p7b
 *   keyAlias      key alias inside the .p12
 *   storePassword plaintext keystore password
 *   keyPassword   plaintext key password
 *   bundleName    bundle name bound in the .p7b release profile
 *   versionName   app version name
 *   versionCode   app version code (integer)
 */
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

// Constant folded into the root-key derivation by DecipherUtil.getRootKey.
const ROOT_KEY_COMPONENT = Buffer.from([
  49, 243, 9, 115, 214, 175, 91, 184, 211, 190, 177, 88, 101, 131, 192, 119,
]);

function fail(message) {
  process.stdout.write(JSON.stringify({ ok: false, error: message }) + '\n');
  process.exit(1);
}

function xorAll(buffers) {
  const out = Buffer.alloc(16);
  buffers[0].copy(out);
  for (let i = 1; i < buffers.length; i++) {
    for (let j = 0; j < 16; j++) out[j] ^= buffers[i][j];
  }
  return out;
}

// Blob layout expected by DecipherUtil.decrypt:
//   [4-byte BE r][12-byte IV][ciphertext][16-byte GCM tag], where r = len - 16.
function encryptBlob(key, plaintext) {
  const iv = crypto.randomBytes(12);
  const cipher = crypto.createCipheriv('aes-128-gcm', key, iv);
  const ciphertext = Buffer.concat([cipher.update(plaintext), cipher.final()]);
  const header = Buffer.alloc(4);
  header.writeUInt32BE(16 + ciphertext.length, 0);
  return Buffer.concat([header, iv, ciphertext, cipher.getAuthTag()]);
}

function decryptBlob(key, blob) {
  const r = blob.readUInt32BE(0);
  const ivLength = blob.length - 4 - r;
  const decipher = crypto.createDecipheriv('aes-128-gcm', key, blob.subarray(4, 4 + ivLength));
  decipher.setAuthTag(blob.subarray(blob.length - 16));
  return Buffer.concat([
    decipher.update(blob.subarray(4 + ivLength, blob.length - 16)),
    decipher.final(),
  ]);
}

// Writes material/{fd/0,fd/1,fd/2,ac,ce} and returns the password-encryption key.
function createSigningMaterial(materialDir) {
  const fd = [crypto.randomBytes(16), crypto.randomBytes(16), crypto.randomBytes(16)];
  const salt = crypto.randomBytes(16);
  const passwordKey = crypto.randomBytes(16);

  const rootKey = crypto.pbkdf2Sync(
    xorAll([...fd, ROOT_KEY_COMPONENT]).toString(),
    salt,
    10000,
    16,
    'sha256',
  );
  const workMaterial = encryptBlob(rootKey, passwordKey);

  // Fail here rather than inside hvigor, whose error message does not mention
  // the material directory at all.
  if (!decryptBlob(rootKey, workMaterial).equals(passwordKey)) {
    throw new Error('signing material self-check failed');
  }

  fs.rmSync(materialDir, { recursive: true, force: true });
  const writeSingleFileEntry = (dir, contents) => {
    fs.mkdirSync(dir, { recursive: true });
    // hvigor requires exactly one file per entry directory; the name is arbitrary.
    fs.writeFileSync(path.join(dir, crypto.randomBytes(16).toString('hex')), contents);
  };
  fd.forEach((value, index) => writeSingleFileEntry(path.join(materialDir, 'fd', String(index)), value));
  writeSingleFileEntry(path.join(materialDir, 'ac'), salt);
  writeSingleFileEntry(path.join(materialDir, 'ce'), workMaterial);

  return passwordKey;
}

// Swaps the whole array literal following "key", tracking bracket depth so that
// nested objects inside the existing value cannot terminate the match early.
function replaceArrayLiteral(source, key, replacement) {
  const keyIndex = source.indexOf(`"${key}"`);
  if (keyIndex < 0) throw new Error(`"${key}" not found`);
  const open = source.indexOf('[', keyIndex);
  if (open < 0) throw new Error(`"${key}" is not followed by an array`);

  let depth = 0;
  let close = -1;
  for (let i = open; i < source.length; i++) {
    if (source[i] === '[') depth++;
    else if (source[i] === ']') {
      depth--;
      if (depth === 0) { close = i; break; }
    }
  }
  if (close < 0) throw new Error(`unbalanced array after "${key}"`);
  return source.slice(0, open) + replacement + source.slice(close + 1);
}

function replaceScalar(source, key, renderedValue) {
  const pattern = new RegExp(`("${key}"\\s*:\\s*)("[^"]*"|-?\\d+)`);
  if (!pattern.test(source)) throw new Error(`"${key}" not found or not a scalar`);
  return source.replace(pattern, `$1${renderedValue}`);
}

function signingConfigBlock(material) {
  const lines = [
    '[',
    '      {',
    '        "name": "default",',
    '        "type": "HarmonyOS",',
    '        "material": {',
    `          "certpath": ${JSON.stringify(material.certpath)},`,
    `          "keyAlias": ${JSON.stringify(material.keyAlias)},`,
    `          "keyPassword": ${JSON.stringify(material.keyPassword)},`,
    `          "profile": ${JSON.stringify(material.profile)},`,
    '          "signAlg": "SHA256withECDSA",',
    `          "storeFile": ${JSON.stringify(material.storeFile)},`,
    `          "storePassword": ${JSON.stringify(material.storePassword)}`,
    '        }',
    '      }',
    '    ]',
  ];
  return lines.join('\n');
}

function main() {
  let request;
  try {
    request = JSON.parse(fs.readFileSync(0, 'utf8'));
  } catch (err) {
    return fail(`invalid JSON request on stdin: ${err.message}`);
  }

  const required = [
    'workDir', 'buildProfile', 'appJson5', 'storeFile', 'certPath',
    'profilePath', 'keyAlias', 'storePassword', 'keyPassword',
    'bundleName', 'versionName', 'versionCode',
  ];
  const missing = required.filter((field) => request[field] === undefined || request[field] === '');
  if (missing.length > 0) return fail(`missing request field(s): ${missing.join(', ')}`);

  try {
    for (const field of ['buildProfile', 'appJson5', 'storeFile', 'certPath', 'profilePath']) {
      if (!fs.existsSync(request[field])) return fail(`file not found (${field}): ${request[field]}`);
    }

    fs.mkdirSync(request.workDir, { recursive: true });

    // hvigor derives the password key from dirname(storeFile)/material, so the
    // .p12 has to live in the scratch dir alongside the generated material.
    // Copying instead of writing into the keystore directory keeps the caller's
    // credential store read-only.
    const storeFile = path.join(request.workDir, path.basename(request.storeFile));
    const certpath = path.join(request.workDir, path.basename(request.certPath));
    const profile = path.join(request.workDir, path.basename(request.profilePath));
    fs.copyFileSync(request.storeFile, storeFile);
    fs.copyFileSync(request.certPath, certpath);
    fs.copyFileSync(request.profilePath, profile);

    const passwordKey = createSigningMaterial(path.join(request.workDir, 'material'));
    const encrypt = (plaintext) => encryptBlob(passwordKey, Buffer.from(plaintext, 'utf-8')).toString('hex');

    const storePassword = encrypt(request.storePassword);
    const keyPassword = encrypt(request.keyPassword);
    // DecipherUtil rejects anything shorter than 32 hex chars.
    for (const [name, value] of [['storePassword', storePassword], ['keyPassword', keyPassword]]) {
      if (value.length < 32) return fail(`generated ${name} ciphertext is shorter than 32 chars`);
    }

    let buildProfile = fs.readFileSync(request.buildProfile, 'utf8');
    const injected = replaceArrayLiteral(
      buildProfile,
      'signingConfigs',
      signingConfigBlock({ certpath, keyAlias: request.keyAlias, keyPassword, profile, storeFile, storePassword }),
    );
    // Point every product at the injected config; the generated name is "default"
    // so a project that already references "default" needs no further change.
    buildProfile = injected.replace(/"signingConfig"(\s*):(\s*)"[^"]*"/g, '"signingConfig"$1:$2"default"');
    fs.writeFileSync(request.buildProfile, buildProfile);

    let appJson5 = fs.readFileSync(request.appJson5, 'utf8');
    appJson5 = replaceScalar(appJson5, 'bundleName', JSON.stringify(request.bundleName));
    appJson5 = replaceScalar(appJson5, 'versionName', JSON.stringify(request.versionName));
    appJson5 = replaceScalar(appJson5, 'versionCode', String(request.versionCode));
    fs.writeFileSync(request.appJson5, appJson5);

    process.stdout.write(JSON.stringify({
      ok: true,
      workDir: request.workDir,
      storeFile,
      certpath,
      profile,
      bundleName: request.bundleName,
      versionName: request.versionName,
      versionCode: request.versionCode,
    }) + '\n');
  } catch (err) {
    return fail(err.message);
  }
}

main();
