import { useEffect, useState } from 'react'
import { BrowserRouter, Navigate, Route, Routes, useNavigate } from 'react-router-dom'
import { ChatProvider } from './context/ChatContext'
import { ChatApp } from './pages/ChatApp'
import { Login } from './pages/Login'
import { Register } from './pages/Register'

function NavListener() {
  const navigate = useNavigate()
  useEffect(() => {
    const fn = (e: Event) => {
      const to = (e as CustomEvent<{ to: string }>).detail?.to
      if (to) navigate(to, { replace: true })
    }
    window.addEventListener('chat-nav', fn)
    return () => window.removeEventListener('chat-nav', fn)
  }, [navigate])
  return null
}

function ToastHost() {
  const [toast, setToast] = useState<{ message: string; ok?: boolean } | null>(null)
  useEffect(() => {
    const fn = (e: Event) => {
      const d = (e as CustomEvent<{ message: string; ok?: boolean }>).detail
      if (d?.message) {
        setToast({ message: d.message, ok: d.ok })
        window.setTimeout(() => setToast(null), 3200)
      }
    }
    window.addEventListener('chat-toast', fn)
    return () => window.removeEventListener('chat-toast', fn)
  }, [])
  if (!toast) return null
  return (
    <div className={toast.ok ? 'wc-toast wc-toast--ok' : 'wc-toast wc-toast--err'} role="status">
      {toast.message}
    </div>
  )
}

export default function App() {
  return (
    <ChatProvider>
      <BrowserRouter>
        <NavListener />
        <ToastHost />
        <Routes>
          <Route path="/" element={<ChatApp />} />
          <Route path="/login" element={<Login />} />
          <Route path="/register" element={<Register />} />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </BrowserRouter>
    </ChatProvider>
  )
}
