import pygame
import sys
import random

# Ініціалізація Pygame
pygame.init()

# Константи
WIDTH, HEIGHT = 800, 400
FPS = 60
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
RED = (255, 0, 0)

# Створення вікна
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Chrome Dinosaur")
clock = pygame.time.Clock()

# Шрифти
font = pygame.font.Font(None, 36)

# Клас для динозавра
class Dinosaur(pygame.sprite.Sprite):
    def __init__(self):
        super().__init__()
        self.image = pygame.Surface((40, 40))
        self.image.fill(BLACK)
        self.rect = self.image.get_rect()
        self.rect.center = (100, HEIGHT - 60)
        self.is_jumping = False
        self.jump_speed = -15
        self.gravity = 1
        self.y_velocity = 0

    def update(self):
        keys = pygame.key.get_pressed()
        if keys[pygame.K_SPACE] and not self.is_jumping:
            self.is_jumping = True
            self.y_velocity = self.jump_speed

        if self.is_jumping:
            self.y_velocity += self.gravity
            self.rect.y += self.y_velocity
            if self.rect.bottom >= HEIGHT - 20:
                self.rect.bottom = HEIGHT - 20
                self.is_jumping = False

# Клас для перешкод
class Obstacle(pygame.sprite.Sprite):
    def __init__(self):
        super().__init__()
        self.image = pygame.Surface((20, 40))
        self.image.fill(RED)
        self.rect = self.image.get_rect()
        self.rect.x = WIDTH
        self.rect.y = HEIGHT - 60

    def update(self):
        self.rect.x -= 5
        if self.rect.right < 0:
            self.rect.x = WIDTH + random.randint(100, 300)

# Клас для гри
class Game:
    def __init__(self):
        self.dino = Dinosaur()
        self.obstacles = pygame.sprite.Group(Obstacle())
        self.all_sprites = pygame.sprite.Group(self.dino, self.obstacles)
        self.running = True
        self.lives = 3
        self.timer = 0

    def reset(self):
        self.__init__()

    def run(self):
        while self.running:
            self.timer += 1 / FPS
            self.events()
            self.update()
            self.draw()
            clock.tick(FPS)

    def events(self):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()

    def update(self):
        self.all_sprites.update()

        # Перевірка на зіткнення
        if pygame.sprite.spritecollide(self.dino, self.obstacles, False):
            self.lives -= 1
            if self.lives <= 0:
                self.running = False

    def draw(self):
        screen.fill(WHITE)
        self.all_sprites.draw(screen)
        self.display_ui()
        pygame.display.flip()

    def display_ui(self):
        timer_text = font.render(f"Time: {int(self.timer)}", True, BLACK)
        lives_text = font.render(f"Lives: {self.lives}", True, BLACK)
        screen.blit(timer_text, (10, 10))
        screen.blit(lives_text, (10, 50))

# Клас для стартового екрана
class StartScreen:
    def __init__(self):
        self.running = True

    def run(self):
        while self.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()
                if event.type == pygame.KEYDOWN and event.key == pygame.K_RETURN:
                    self.running = False

            screen.fill(WHITE)
            title_text = font.render("Chrome Dinosaur", True, BLACK)
            start_text = font.render("Press ENTER to Start", True, BLACK)
            screen.blit(title_text, (WIDTH // 2 - title_text.get_width() // 2, HEIGHT // 3))
            screen.blit(start_text, (WIDTH // 2 - start_text.get_width() // 2, HEIGHT // 2))
            pygame.display.flip()

# Основний цикл
while True:
    start_screen = StartScreen()
    start_screen.run()

    game = Game()
    game.run()
