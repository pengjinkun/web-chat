import { useEffect, useState } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { useChat } from '../context/ChatContext'
import './Auth.css'

export function Register() {
  const { register, state } = useChat()
  const navigate = useNavigate()
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [password2, setPassword2] = useState('')
  const [localError, setLocalError] = useState('')

  useEffect(() => {
    if (state.user) navigate('/', { replace: true })
  }, [state.user, navigate])

  const submit = (e: React.FormEvent) => {
    e.preventDefault()
    setLocalError('')
    if (!/^[a-zA-Z0-9]{3,16}$/.test(username)) {
      setLocalError('用户名须为 3~16 位字母或数字')
      return
    }
    if (password.length < 6 || password.length > 16) {
      setLocalError('密码长度须为 6~16 位')
      return
    }
    if (password !== password2) {
      setLocalError('两次密码不一致')
      return
    }
    if (!state.connected) {
      setLocalError('未连接到服务器，请确认后端已启动')
      return
    }
    register(username, password, username)
  }

  return (
    <div className="wc-auth">
      <div className="wc-auth__card">
        <h1 className="wc-auth__brand">Web Chat</h1>
        <h2 className="wc-auth__title">用户注册</h2>
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
              autoComplete="new-password"
            />
          </label>
          <label className="wc-auth__label">
            确认密码
            <input
              className="wc-input"
              type="password"
              value={password2}
              onChange={(e) => setPassword2(e.target.value)}
              autoComplete="new-password"
            />
          </label>
          <button type="submit" className="wc-btn wc-btn--primary wc-auth__submit" disabled={!state.connected}>
            注册
          </button>
        </form>
        <p className="wc-auth__footer">
          已有账号？<Link to="/login">前往登录</Link>
        </p>
      </div>
    </div>
  )
}
