import { useEffect, useMemo, useRef, useState } from 'react'
import { useChat } from '../context/ChatContext'
import { keyForPeer } from '../context/chatReducer'
import { MessageItem } from './MessageItem'
import './ChatWindow.css'

export function ChatWindow() {
  const { state, send } = useChat()
  const [text, setText] = useState('')
  const bottomRef = useRef<HTMLDivElement>(null)
  const session = state.currentSession
  const uid = state.user?.id

  const msgKey = useMemo(() => {
    if (!session) return null
    return keyForPeer(session.type, session.id)
  }, [session])

  const messages = msgKey ? state.messagesByKey[msgKey] ?? [] : []

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [messages.length, session?.id, session?.type])

  const peerName = useMemo(() => {
    if (!session || session.type !== 'p2p') return undefined
    const f = state.friends.find((x) => x.id === session.id)
    return f?.nickname || f?.username
  }, [session, state.friends])

  const sendMsg = () => {
    const v = text.trim()
    if (!v || !session || !uid) return
    if (session.type === 'p2p') {
      send('chat.p2p', { toUid: session.id, content: v })
    } else {
      send('chat.group', { groupId: session.id, content: v })
    }
    setText('')
  }

  const offlineHint = !state.connected

  if (!session) {
    return (
      <main className="wc-chat wc-chat--empty">
        <p>选择好友或群聊开始对话</p>
      </main>
    )
  }

  return (
    <main className="wc-chat">
      <header className="wc-chat__head">
        <div className="wc-chat__title">{session.title}</div>
        {offlineHint && <div className="wc-chat__offline">网络已断开，正在重连…</div>}
      </header>
      <div className="wc-chat__scroll">
        {messages.map((m) => (
          <MessageItem key={m.id} message={m} selfId={uid!} peerName={peerName} />
        ))}
        <div ref={bottomRef} />
      </div>
      <footer className="wc-chat__foot">
        <textarea
          className="wc-textarea"
          rows={2}
          placeholder="输入消息，Enter 发送"
          value={text}
          onChange={(e) => setText(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
              e.preventDefault()
              sendMsg()
            }
          }}
        />
        <button type="button" className="wc-btn wc-btn--primary" onClick={sendMsg}>
          发送
        </button>
      </footer>
    </main>
  )
}
