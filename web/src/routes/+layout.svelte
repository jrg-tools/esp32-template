<script lang="ts">
	import favicon from '$lib/assets/favicon.svg';
	import '../app.css';

	let { children } = $props();
</script>

<svelte:head>
	<link rel="icon" href={favicon} />
</svelte:head>

<main class="app">
	{@render children()}
</main>

<style>
	.app {
		max-width: 520px;
		margin: 40px auto;
		padding: 0 16px;
	}
</style>
