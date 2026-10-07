# web

SvelteKit UI served by the device from its LittleFS partition. Built as a
fully static site (`@sveltejs/adapter-static`), so there is no server runtime
on the device — the firmware only serves files and the `/api/*` JSON routes.

```sh
pnpm install
pnpm dev       # dev server; /api is proxied to http://$DEVICE_HOST (default esp32.local)
pnpm check     # type-check
pnpm deploy    # build and stage into ../data for `pio run -t uploadfs`
```
