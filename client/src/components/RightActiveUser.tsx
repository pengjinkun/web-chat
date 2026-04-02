import { useEffect, useState } from 'react'
import { useChat } from '../context/ChatContext'
import './RightActiveUser.css'

function formatDuration(sec: number): string {
  if (sec < 60) return `${sec}秒`
  if (sec < 3600) return `${Math.floor(sec / 60)}分钟`
  return `${Math.floor(sec / 3600)}小时${Math.floor((sec % 3600) / 60)}分`
}

export function RightActiveUser() {
  const { state, send } = useChat()
  const [searchQ, setSearchQ] = useState('')

  useEffect(() => {
    send('friend.recommend', {})
    const t = window.setInterval(() => send('friend.recommend', {}), 30_000)
    return () => window.clearInterval(t)
  }, [send])

  const friendIds = new Set(state.friends.map((f) => f.id))

  const addFriend = (id: number) => {
    send('friend.add', { toUid: id })
    window.dispatchEvent(
      new CustomEvent('chat-toast', { detail: { message: '已发送好友申请', ok: true } })
    )
  }

  const runSearch = () => {
    send('friend.search', { keyword: searchQ.trim() })
  }

  return (
    <aside className="wc-right">
      <div className="wc-right__head">找好友</div>
      <div className="wc-right__search">
        <input
          className="wc-input wc-input--sm"
          placeholder="用户名模糊搜索"
          value={searchQ}
          onChange={(e) => setSearchQ(e.target.value)}
          onKeyDown={(e) => e.key === 'Enter' && runSearch()}
        />
        <button type="button" className="wc-btn wc-btn--secondary wc-btn--sm" onClick={runSearch}>
          搜索
        </button>
      </div>
      {state.friendSearchResults.length > 0 && (
        <ul className="wc-right__search-list">
          {state.friendSearchResults.map((u) => {
            const already = friendIds.has(u.id)
            const self = state.user?.id === u.id
            return (
              <li key={u.id} className="wc-right__item">
                <div>
                  <div className="wc-right__name">{u.nickname || u.username}</div>
                </div>
                <button
                  type="button"
                  className="wc-btn wc-btn--sm"
                  disabled={already || self}
                  onClick={() => addFriend(u.id)}
                >
                  {self ? '本人' : already ? '已是好友' : '添加'}
                </button>
              </li>
            )
          })}
        </ul>
      )}

      <div className="wc-right__head wc-right__head--sub">在线推荐</div>
      <p className="wc-right__hint">按在线时长排序 · 每 30 秒刷新</p>
      <ul className="wc-right__list">
        {state.activeRecommend.map((u) => {
          const already = friendIds.has(u.id)
          return (
            <li key={u.id} className="wc-right__item">
              <div>
                <div className="wc-right__name">{u.nickname || u.username}</div>
                <div className="wc-right__time">在线 {formatDuration(u.onlineSeconds)}</div>
              </div>
              <button
                type="button"
                className="wc-btn wc-btn--sm"
                disabled={already}
                onClick={() => addFriend(u.id)}
              >
                {already ? '已是好友' : '添加好友'}
              </button>
            </li>
          )
        })}
      </ul>
      {state.activeRecommend.length === 0 && (
        <div className="wc-right__empty">暂无其他在线用户</div>
      )}
    </aside>
  )
}
