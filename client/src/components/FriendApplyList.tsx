import { useEffect } from 'react'
import { useChat } from '../context/ChatContext'
import './FriendApplyList.css'

export function FriendApplyList({ open, onClose }: { open: boolean; onClose: () => void }) {
  const { state, send } = useChat()
  const list = state.applyList

  useEffect(() => {
    if (!open) return
    send('friend.apply.read', {})
    send('friend.apply.list', {})
  }, [open, send])

  if (!open) return null

  const agree = (applyId: number) => {
    send('friend.agree', { applyId })
  }

  const reject = (applyId: number) => {
    send('friend.apply.reject', { applyId })
  }

  const markAllRead = () => {
    send('friend.apply.read', {})
  }

  return (
    <div className="wc-drawer-backdrop" role="presentation" onClick={onClose}>
      <div className="wc-drawer" role="dialog" onClick={(e) => e.stopPropagation()}>
        <div className="wc-drawer__head">
          <span>好友申请</span>
          <div className="wc-drawer__actions">
            <button type="button" className="wc-btn wc-btn--ghost" onClick={markAllRead}>
              全部已读
            </button>
            <button type="button" className="wc-btn wc-btn--ghost" onClick={onClose}>
              关闭
            </button>
          </div>
        </div>
        <ul className="wc-drawer__list">
          {list.map((row) => {
            const pending = row.status === 0
            const done = row.status !== 0
            return (
              <li key={row.id} className={done ? 'wc-apply wc-apply--done' : 'wc-apply'}>
                <div className="wc-apply__text">
                  <strong>{row.nickname || row.username}</strong> 请求添加你为好友
                </div>
                {pending ? (
                  <div className="wc-apply__btns">
                    <button type="button" className="wc-btn wc-btn--primary" onClick={() => agree(row.id)}>
                      同意
                    </button>
                    <button type="button" className="wc-btn wc-btn--secondary" onClick={() => reject(row.id)}>
                      拒绝
                    </button>
                  </div>
                ) : (
                  <div className="wc-apply__status">{row.status === 1 ? '已同意' : '已拒绝'}</div>
                )}
              </li>
            )
          })}
        </ul>
        {list.length === 0 && <div className="wc-drawer__empty">暂无申请记录</div>}
      </div>
    </div>
  )
}
