import adapter from '@sveltejs/adapter-static';
import { sveltekit } from '@sveltejs/kit/vite';
import { defineConfig } from 'vite';

export default defineConfig({
	plugins: [
		sveltekit({
			compilerOptions: {
				// Force runes mode for the project, except for libraries. Can be removed in svelte 6.
				runes: ({ filename }) =>
					filename.split(/[/\\]/).includes('node_modules') ? undefined : true
			},

			// Static build served by the device from LittleFS. `fallback` gives
			// dynamic routes (ids only known at runtime) a client-rendered shell;
			// ConfigWebServer serves that file for any unmatched non-API path
			// (see handleNotFound).
			adapter: adapter({ fallback: '200.html' })
		})
	],
	server: {
		// During `pnpm dev`, proxy API calls to a real device.
		proxy: {
			'/api': `http://${process.env.DEVICE_HOST ?? 'esp32.local'}`
		}
	}
});
