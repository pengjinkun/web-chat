import type { ChatMessage } from '../types'
import './MessageItem.css'

export function MessageItem({
  message,
  selfId,
  peerName,
}: {
  message: ChatMessage
  selfId: number
  peerName?: string
}) {
  const isSystem = message.msgType === 2
  if (isSystem) {
    return (
      <div className="wc-msg wc-msg--system">
        <span>{message.content}</span>
      </div>
    )
  }
  const mine = message.fromUid === selfId
  return (
    <div className={`wc-msg wc-msg--row ${mine ? 'wc-msg--mine' : ''}`}>
      {!mine && <div className="wc-msg__who">{peerName ?? `用户${message.fromUid}`}</div>}
      <div className="wc-msg__bubble">{message.content}</div>
    </div>
  )
}
