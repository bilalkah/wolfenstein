# Downloading and caching

## Purpose

The game is about 18 MB (3 MB of code, 15 MB of assets). The page must
download it quickly on a good connection, keep going on a lossy one (the
game is also played from a LAN server over Wi-Fi), avoid downloading it
again on the next visit, recover from a broken or stale copy, and show
progress while it waits. All of this lives in `web/shell.html`, in plain
JavaScript around Emscripten's loader.

## Concepts

### Byte ranges and parallel connections

An HTTP server that answers `Accept-Ranges: bytes` lets a client ask for a
piece of a file (`Range: bytes=1048576-2097151`, answered `206 Partial
Content`). Download managers use this to fetch several pieces at once over
several connections and to resume a piece from where it stopped. On a
lossy network a single TCP connection that hits a bad patch backs off for
seconds at a time, even after the network recovers; a *new* connection
does not.

### Content encoding

A server may compress a response on the way (`Content-Encoding: gzip`);
the browser inflates it transparently. The `Content-Length` header is
then the **compressed** size, and a byte range is a range of the
**compressed** bytes. GitHub Pages compresses `.wasm` and `.data` files
(checked with `curl` against a GitHub Pages site on 2026-09-29: `.wasm`
and `application/octet-stream` responses come with `content-encoding:
gzip`, and a `Range` request returns a slice of the gzip stream).

## How it is implemented here

### Taking over the downloads

```javascript title="web/shell.html"
  window.fetch = (input, init) =>
    typeof input === 'string' && input.endsWith('index.data')
      ? cachedFetch(input, 'application/octet-stream', (loaded, total) => {
          progress.data = [loaded, total];
          showProgress();
        })
      : nativeFetch(input, init);
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/web/shell.html#L362-L368){ .excerpt-source }

Emscripten's generated loader fetches `index.data` with `fetch`, so the
page replaces `window.fetch` for that one URL; `index.wasm` goes through
`Module.instantiateWasm`, which the page also provides, streaming the
download into `WebAssembly.instantiateStreaming`. Both use `cachedFetch`.

### Keep, or fetch

```javascript title="web/shell.html"
  async function cachedFetch(url, type, onProgress) {
    const remote = await remoteFile(url);
    if (!remote) return nativeFetch(url);  // unknown: fetched as it is
    const kept = await inStore('readonly', (files) => files.get(url)).catch(() => undefined);
    if (kept?.version === remote.version) {
      onProgress(kept.blob.size, kept.blob.size);
      return new Response(kept.blob, {
        headers: { 'Content-Type': type, 'Content-Length': String(kept.blob.size) },
      });
    }
    const response = remote.ranges
      ? piecedFetch(url, type, remote.size, remote.stamp, onProgress)
      : await wholeFetch(url, type, remote, onProgress);
    if (!response.ok) return response;
    // Kept only if every piece arrived: a failed download is fetched afresh
    // next time
    const [forGame, forStore] = response.body.tee();
    new Response(forStore).blob()
      .then((blob) => inStore('readwrite', (files) =>
        files.put({ version: remote.version, blob }, url)))
      .catch((error) => console.warn(`${url}: not kept for the next visit:`, error));
    return new Response(forGame, { headers: response.headers });
  }
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/web/shell.html#L311-L333){ .excerpt-source }

1. A `HEAD` request (`remoteFile`) learns the file's size and its date or
   tag, which together identify the build, whether ranges are offered, and
   whether the server compresses it.
2. A copy kept in **IndexedDB** with the same version is used as is: the
   next visit after a build downloads nothing. (IndexedDB rather than the
   Cache API, which needs HTTPS: the game is also played from plain
   `http://` LAN addresses.)
3. Otherwise the file is downloaded (in pieces, or whole) and **teed**:
   one stream to the game, one into a blob that is stored only if every
   byte arrived.

### In pieces, with stalls resumed

`piecedFetch` splits the file into 1 MB pieces fetched by four workers,
each piece with `Range` requests. A request that brings nothing for two
seconds is aborted and the rest of the piece asked for again from the byte
it reached, on a new connection; thirty requests in a row with no progress
give up. Every response must carry the same date or tag as the `HEAD`
(else the build changed mid-download, a fatal error). The pieces fill one
`Uint8Array`, and the response body streams them out in order as they
complete, so compiling the wasm can start before the last piece arrives.

### Whole, for servers that compress (GitHub Pages)

Pieces only work on the file's own bytes. When the server compresses a
file, the page fetches it **whole**, lets the browser inflate it, and
counts the inflated bytes for the progress text:

```javascript title="web/shell.html"
  async function wholeFetch(url, type, remote, onProgress) {
    const response = await nativeFetch(url, { cache: 'no-store' });
    if (!response.ok || !response.body) return response;
    const total = remote.compressed ? 0 : remote.size;
    let received = 0;
    onProgress(0, total);
    const counted = response.body.pipeThrough(new TransformStream({
      transform(chunk, stream) {
        received += chunk.length;
        onProgress(received, total);
        stream.enqueue(chunk);
      },
    }));
    return new Response(counted, { headers: { 'Content-Type': type } });
  }
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/web/shell.html#L295-L309){ .excerpt-source }

!!! bug "Fixed while writing these docs: the game did not start on GitHub Pages"
    Before `73aaf65` ("Load the game from a server that compresses it"),
    the page downloaded in pieces whenever the server offered ranges.
    GitHub Pages offers ranges *and* gzips the files, so the pieces were
    slices of a gzip stream sized by the compressed length:
    `WebAssembly.instantiateStreaming` failed with "section extends past
    end of the module" and the game never started. Reproduced and
    verified with a local server that serves like GitHub Pages
    (compression, ranges over the compressed bytes, weak ETags): the
    unfixed page failed, the fixed page loads, and the uncompressed path
    (`serve_web.py`) still downloads in pieces.

With compression the total size is unknown (only the compressed length
is), so the loader shows how many megabytes have arrived instead of a
fraction.

### Healing a broken start

A game that fails to start may have met a stale or broken kept copy. The
page watches for `onGameReady` (called by `Game::Run`); if it has not come
a second after the runtime is ready, or the runtime aborts, the page
**clears its kept copies and reloads once** (remembered in
`sessionStorage`). Only a second failure is shown to the player.

```javascript title="web/shell.html"
  async function recover(text) {
    if (recovering) return;
    if (running || session?.getItem(kRecoveredKey)) {
      fail(text);
      return;
    }
    recovering = true;
    session?.setItem(kRecoveredKey, '1');
    loaderEl.hidden = false;
    statusEl.textContent = 'Updating the game…';
    await inStore('readwrite', (files) => files.clear()).catch(() => {});
    location.reload();
  }
```
[View on GitHub](https://github.com/bilalkah/wolfenstein/blob/73aaf653bbc3b5dbb26be73432bb845df5e76c52/web/shell.html#L346-L358){ .excerpt-source }

## Design decisions and trade-offs

- **The page's own downloader.** More code than letting the browser fetch,
  but it made the game playable over the lossy Wi-Fi it was tested on
  (`de89d1c`, "Download in resumable pieces, and reset connections that
  stop"; `de89d1c`, "Resume stalled downloads instead of starting chunks
  over").
- **IndexedDB keeps whole files, keyed by URL and version.** Simple, and
  a changed build is detected by one `HEAD` per file.
- **No service worker.** Everything happens in the page; there is nothing
  to install or update separately.

## Pitfalls

- **Servers differ.** Ranges, compression and validators (`Last-Modified`,
  `ETag`) all change which path runs. A server with neither validator
  (the benchmark's test server) gets a plain `fetch` with no caching.
- **Storage can be refused** (private windows, quotas); every IndexedDB
  call tolerates failure and the game still loads, just without keeping a
  copy.
- **Two builds must never mix.** The version check per file and the stamp
  check per piece prevent a new `index.wasm` running with an old
  `index.data`; the store was versioned (`indexedDB.open('wolfenstein', 2)`)
  to drop copies an older page version kept without those checks.
