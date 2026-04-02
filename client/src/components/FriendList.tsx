import { useMemo, useState } from 'react'
import { useChat } from '../context/ChatContext'
import type { Friend } from '../types'
import { Badge } from './Badge'
import './FriendList.css'

export function FriendList({
  onOpenApply,
}: {
  onOpenApply: () => void
}) {
  const { state, openSession } = useChat()
  const [q, setQ] = useState('')

  const filtered = useMemo(() => {
    const list = state.friends
    if (!q.trim()) return list
    const s = q.trim().toLowerCase()
    return list.filter((f) => f.username.toLowerCase().includes(s) || f.nickname.toLowerCase().includes(s))
  }, [state.friends, q])

  const sorted = useMemo(() => {
    return [...filtered].sort((a, b) => a.username.localeCompare(b.username))
  }, [filtered])

  const selectFriend = (f: Friend) => {
    openSession({ type: 'p2p', id: f.id, title: f.nickname || f.username })
  }

  return (
    <div className="wc-friendlist">
      <div className="wc-friendlist__toolbar">
        <input
          className="wc-input wc-input--sm"
          placeholder="搜索好友"
          value={q}
          onChange={(e) => setQ(e.target.value)}
        />
        <button type="button" className="wc-icon-btn" title="好友申请" onClick={onOpenApply}>
          🔔
          <Badge count={state.pendingApplyCount || 1} hidden={!state.applyBell} />
        </button>
      </div>
      <ul className="wc-friendlist__ul">
        {sorted.map((f) => {
          const unread = state.unreadP2p[String(f.id)] ?? 0
          const active =
            state.currentSession?.type === 'p2p' && state.currentSession.id === f.id
          return (
            <li key={f.id}>
              <button
                type="button"
                className={`wc-row ${active ? 'wc-row--active' : ''}`}
                onClick={() => selectFriend(f)}
              >
                <span className="wc-row__title">{f.nickname || f.username}</span>
                <Badge count={unread} hidden={active} />
              </button>
            </li>
          )
        })}
      </ul>
    </div>
  )
}
