import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    // 让 Vite devserver 监听公网/局域网网卡，供远程访问
    host: true,
    proxy: {
      '/ws': {
        target: 'ws://43.155.207.211:8765',
        ws: true,
      },
    },
  },
})
