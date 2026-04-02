import { useEffect, useState } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { useChat } from '../context/ChatContext'
import './Auth.css'

export function Login() {
  const { login, state } = useChat()
  const navigate = useNavigate()
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [localError, setLocalError] = useState('')

  useEffect(() => {
    if (state.user) navigate('/', { replace: true })
  }, [state.user, navigate])

  const submit = (e: React.FormEvent) => {
    e.preventDefault()
    setLocalError('')
    if (!username.trim() || !password) {
      setLocalError('请输入用户名和密码')
      return
    }
    if (!state.connected) {
      setLocalError('未连接到服务器')
      return
    }
    login(username.trim(), password)
  }

  return (
    <div className="wc-auth">
      <div className="wc-auth__card">
        <h1 className="wc-auth__brand">Web Chat</h1>
        <h2 className="wc-auth__title">登录</h2>
        <form onSubmit={submit} className="wc-auth__form">
          {localError && <div className="wc-auth__err">{localError}</div>}
          <label className="wc-auth__label">
            用户名
            <input
              className="wc-input"
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              autoComplete="username"
            />
          </label>
          <label className="wc-auth__label">
            密码
            <input
              className="wc-input"
              type="password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              autoComplete="current-password"
            />
          </label>
          <button type="submit" className="wc-btn wc-btn--primary wc-auth__submit" disabled={!state.connected}>
            登录
          </button>
        </form>
        <p className="wc-auth__footer">
          没有账号？<Link to="/register">去注册</Link>
        </p>
      </div>
    </div>
  )
}
