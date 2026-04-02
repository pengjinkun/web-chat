import { useState } from 'react'
import { FriendList } from './FriendList'
import { GroupList } from './GroupList'
import './LeftSidebar.css'

type Tab = 'friends' | 'groups'

export function LeftSidebar({ onOpenApply }: { onOpenApply: () => void }) {
  const [tab, setTab] = useState<Tab>('friends')

  return (
    <aside className="wc-left">
      <div className="wc-left__tabs">
        <button
          type="button"
          className={tab === 'friends' ? 'wc-tab wc-tab--on' : 'wc-tab'}
          onClick={() => setTab('friends')}
        >
          好友
        </button>
        <button
          type="button"
          className={tab === 'groups' ? 'wc-tab wc-tab--on' : 'wc-tab'}
          onClick={() => setTab('groups')}
        >
          群聊
        </button>
      </div>
      <div className="wc-left__body">{tab === 'friends' ? <FriendList onOpenApply={onOpenApply} /> : <GroupList />}</div>
    </aside>
  )
}
