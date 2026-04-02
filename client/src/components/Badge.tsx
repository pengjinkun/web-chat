import './Badge.css'

export function Badge({ count, hidden }: { count: number; hidden?: boolean }) {
  if (hidden || count <= 0) return null
  const text = count > 99 ? '99+' : String(count)
  return <span className="wc-badge">{text}</span>
}
