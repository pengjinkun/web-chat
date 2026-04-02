import { useMemo, useState } from 'react'
import { useChat } from '../context/ChatContext'
import { Badge } from './Badge'
import './FriendList.css'
import './GroupList.css'

export function GroupList() {
  const { state, openSession, send } = useChat()
  const [q, setQ] = useState('')
  const [creating, setCreating] = useState(false)
  const [newName, setNewName] = useState('')
  const [joinId, setJoinId] = useState('')

  const filtered = useMemo(() => {
    const list = state.groups
    if (!q.trim()) return list
    const s = q.trim().toLowerCase()
    return list.filter((g) => g.group_name.toLowerCase().includes(s) || String(g.id).includes(s))
  }, [state.groups, q])

  const selectGroup = (g: (typeof state.groups)[0]) => {
    openSession({ type: 'group', id: g.id, title: g.group_name })
  }

  const createGroup = () => {
    const name = newName.trim()
    if (!name) return
    send('group.create', { groupName: name })
    setNewName('')
    setCreating(false)
  }

  const joinGroup = () => {
    const id = Number(joinId.trim())
    if (!id) return
    send('group.join', { groupId: id })
    setJoinId('')
  }

  return (
    <div className="wc-friendlist">
      <div className="wc-friendlist__toolbar">
        <input
          className="wc-input wc-input--sm"
          placeholder="搜索群名称或群ID"
          value={q}
          onChange={(e) => setQ(e.target.value)}
        />
      </div>
      <div className="wc-group-actions wc-group-actions--row">
        <button type="button" className="wc-btn wc-btn--ghost" onClick={() => setCreating((v) => !v)}>
          {creating ? '取消' : '创建群聊'}
        </button>
        <div className="wc-join-inline">
          <input
            className="wc-input wc-input--sm"
            placeholder="群ID加入"
            value={joinId}
            onChange={(e) => setJoinId(e.target.value)}
          />
          <button type="button" className="wc-btn wc-btn--secondary" onClick={joinGroup}>
            加入
          </button>
        </div>
      </div>
      {creating && (
        <div className="wc-create-row">
          <input
            className="wc-input wc-input--sm"
            placeholder="群名称"
            value={newName}
            onChange={(e) => setNewName(e.target.value)}
          />
          <button type="button" className="wc-btn wc-btn--primary" onClick={createGroup}>
            确定
          </button>
        </div>
      )}
      <ul className="wc-friendlist__ul">
        {filtered.map((g) => {
          const unread = state.unreadGroup[String(g.id)] ?? 0
          const active =
            state.currentSession?.type === 'group' && state.currentSession.id === g.id
          return (
            <li key={g.id}>
              <button
                type="button"
                className={`wc-row ${active ? 'wc-row--active' : ''}`}
                onClick={() => selectGroup(g)}
              >
                <span className="wc-row__title">{g.group_name}</span>
                <span className="wc-row__meta">#{g.id}</span>
                <Badge count={unread} hidden={active} />
              </button>
            </li>
          )
        })}
      </ul>
    </div>
  )
}
