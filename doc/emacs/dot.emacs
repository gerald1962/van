
;; Added by Package.el.  This must come before configurations of
;; installed packages.  Don't delete this line.  If you don't want it,
;; just comment it out by adding a semicolon to the start of the line.
;; You may delete these explanatory comments.
;; (package-initialize)

(unless (package-installed-p 'use-package)
  (package-refresh-contents)
  (package-install 'use-package))

(require 'rust-mode)

(defun linux-c-mode ()
 "C mode with adjusted defaults for use with the Linux kernel."
 (interactive)
 (c-mode)
 (setq c-indent-level 8)
 (setq c-brace-imaginary-offset 0)
 (setq c-brace-offset -8)
 (setq c-argdecl-indent 8)
 (setq c-label-offset -8)
 (setq c-continued-statement-offset 8)
 (setq indent-tabs-mode nil)
 (setq tab-width 8))


; This will define the M-x linux-c-mode command. When hacking on a module, if
; you put the string -*- linux-c -*- somewhere on the first two lines, this mode
; will be automatically invoked. Also, you may want to add
(setq auto-mode-alist (cons '("/usr/src/linux.*/.*\\.[ch]$" . linux-c-mode)  auto-mode-alist))

; Use Linux C mode in literate programming with cweb file extension x.w
; The full syntax for specifying file extensions for linux-c-mode is:
(add-to-list 'auto-mode-alist '("\\.w\\'" . linux-c-mode))

(setq-default c-default-style "linux")

(setq column-number-mode t)
(helm-mode 1)
(load-theme 'melancholy t)
(set-face-attribute 'default nil :height 125)

(require 'package)
(add-to-list 'package-archives '("melpa" . "https://melpa.org/packages/") t)
;; Comment/uncomment this line to enable MELPA Stable if desired.  See `package-archive-priorities`
;; and `package-pinned-packages`. Most users will not need or want to do this.
;;(add-to-list 'package-archives '("melpa-stable" . "https://stable.melpa.org/packages/") t)
(package-initialize)
(package-refresh-contents)

'(gdb-mi :fetcher git
         :url "https://github.com/weirdNox/emacs-gdb.git"
         :files ("*.el" "*.c" "*.h" "Makefile"))


(custom-set-variables
 ;; custom-set-variables was added by Custom.
 ;; If you edit it by hand, you could mess it up, so be careful.
 ;; Your init file should contain only one such instance.
 ;; If there is more than one, they won't work right.
 '(package-selected-packages
   '(csharp-mode plantuml-mode ob-napkin dap-mode exec-path-from-shell yasnippet company lsp-ui helm-company lsp-mode rustic rust-auto-use flycheck-rust rust-playground rust-mode magit melancholy-theme helm)))

(custom-set-faces
 ;; custom-set-faces was added by Custom.
 ;; If you edit it by hand, you could mess it up, so be careful.
 ;; Your init file should contain only one such instance.
 ;; If there is more than one, they won't work right.
 )


(put 'downcase-region 'disabled nil)


;; Sample jar configuration
(setq plantuml-jar-path "/usr/share/plantuml/plantuml.jar")
(setq plantuml-default-exec-mode 'jar)

;; Sample executable configuration
(setq plantuml-executable-path "plantuml")
(setq plantuml-default-exec-mode 'executable)

;; Enable plantuml-mode for PlantUML files
(add-to-list 'auto-mode-alist '("\\.plantuml\\'" . plantuml-mode))

;; Change the colors for the plantuml mode
(defun plantuml-colors ()
  "Foreground and background colors for the plantuml-mode"
  (interactive)
  (set-foreground-color "black")
  (set-background-color "LightGrey"))

(add-hook 'plantuml-mode-hook 'plantuml-colors)


;; This is a mode for editing C# in emacs. 
(defun my-csharp-mode-hook ()
  ;; enable the stuff you want for C# here
  (electric-pair-mode 1)       ;; Emacs 24
  (electric-pair-local-mode 1) ;; Emacs 25
  )
(add-hook 'csharp-mode-hook 'my-csharp-mode-hook)
