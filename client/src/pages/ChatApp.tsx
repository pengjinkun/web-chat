import { useState } from 'react'
import { Navigate } from 'react-router-dom'
import { useChat } from '../context/ChatContext'
import { LeftSidebar } from '../components/LeftSidebar'
import { ChatWindow } from '../components/ChatWindow'
import { RightActiveUser } from '../components/RightActiveUser'
import { FriendApplyList } from '../components/FriendApplyList'
import './ChatApp.css'

export function ChatApp() {
  const { state, logout } = useChat()
  const [applyOpen, setApplyOpen] = useState(false)

  if (!state.user) {
    return <Navigate to="/login" replace />
  }

  return (
    <div className="wc-app">
      <header className="wc-app__top">
        <span className="wc-app__logo">Web Chat</span>
        <div className="wc-app__user">
          <span>{state.user.nickname || state.user.username}</span>
          <button type="button" className="wc-btn wc-btn--ghost" onClick={logout}>
            退出
          </button>
        </div>
      </header>
      <div className="wc-app__main">
        <div className="wc-app__col wc-app__col--left">
          <LeftSidebar onOpenApply={() => setApplyOpen(true)} />
        </div>
        <div className="wc-app__col wc-app__col--center">
          <ChatWindow />
        </div>
        <div className="wc-app__col wc-app__col--right">
          <RightActiveUser />
        </div>
      </div>
      <FriendApplyList open={applyOpen} onClose={() => setApplyOpen(false)} />
    </div>
  )
}
