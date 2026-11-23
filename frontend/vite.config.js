import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// The C++ Drogon engine listens on :8080. In development every /api and /ws
// request is proxied there, so the browser talks to one origin.
const target = process.env.NEXA_API || 'http://localhost:8080';

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      '/api': { target, changeOrigin: true },
      '/ws': { target: target.replace(/^http/, 'ws'), ws: true },
    },
  },
  build: {
    outDir: 'dist',
    chunkSizeWarningLimit: 1600,
  },
});
