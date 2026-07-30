import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from "path";

// Build output goes to ../server/static so the FastAPI server
// (playground/studio/server/server.py, STATIC_DIR = <server pkg>/static) serves
// the SPA directly. During dev, /api requests are proxied to the local agent.
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
    },
  },
  build: {
    outDir: path.resolve(__dirname, "../server/static"),
    emptyOutDir: true,
  },
  server: {
    port: 5173,
    proxy: {
      "/api": {
        target: "http://127.0.0.1:8765",
        changeOrigin: true,
      },
    },
  },
});
