<script lang="ts">
	import { browser } from '$app/environment';

	// Mirrors ConfigWebServer::handleHealth().
	interface Health {
		status: string;
		version: string;
		ip: string;
		mode: 'AP' | 'STA';
		rssi: number;
		freeHeap: number;
		uptime: number;
	}

	const POLL_MS = 5000;

	let health = $state<Health | null>(null);
	let connected = $state<boolean | null>(null);

	async function checkHealth() {
		if (document.visibilityState === 'hidden') return;
		try {
			const res = await fetch('/api/health');
			if (!res.ok) throw new Error();
			health = await res.json();
			connected = true;
		} catch {
			health = null;
			connected = false;
		}
	}

	$effect(() => {
		if (!browser) return;
		checkHealth();
		const timer = setInterval(checkHealth, POLL_MS);
		return () => clearInterval(timer);
	});
</script>

<svelte:head>
	<title>ESP32</title>
</svelte:head>

<header>
	<h1>ESP32</h1>
	<span class="status" class:ok={connected === true} class:bad={connected === false}>
		{connected === null ? 'Checking…' : connected ? 'Online' : 'Offline'}
	</span>
</header>

{#if health}
	<dl>
		<dt>Version</dt>
		<dd>{health.version}</dd>
		<dt>IP</dt>
		<dd>{health.ip} ({health.mode})</dd>
		{#if health.mode === 'STA'}
			<dt>RSSI</dt>
			<dd>{health.rssi} dBm</dd>
		{/if}
		<dt>Free heap</dt>
		<dd>{(health.freeHeap / 1024).toFixed(1)} KiB</dd>
		<dt>Uptime</dt>
		<dd>{health.uptime} s</dd>
	</dl>
{/if}

<style>
	header {
		display: flex;
		align-items: baseline;
		justify-content: space-between;
		padding-bottom: 12px;
		margin-bottom: 20px;
		border-bottom: 1px solid var(--line);
	}

	h1 {
		margin: 0;
		font-size: 24px;
	}

	.status {
		color: var(--dim);
	}

	.status.ok {
		color: var(--ok);
	}

	.status.bad {
		color: var(--bad);
	}

	dl {
		display: grid;
		grid-template-columns: max-content 1fr;
		gap: 6px 16px;
		margin: 0;
	}

	dt {
		color: var(--dim);
	}

	dd {
		margin: 0;
		font-family: var(--mono);
	}
</style>
